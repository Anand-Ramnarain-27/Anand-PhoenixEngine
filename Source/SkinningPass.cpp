#include "Globals.h"
#include "SkinningPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ComponentAnimation.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "GameObject.h"
#include "Mesh.h"
#include "ResourceMesh.h"
#include "MeshEntry.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <algorithm>
#include <cstring>

static constexpr uint32_t STRUCT_VERTEX_BYTES = sizeof(Mesh::Vertex);

static uint32_t collectJointPalette(
    GameObject* root,
    Matrix* dst,
    uint32_t       maxMatrices)
{
    if (!root) return 0;
    uint32_t count = 0;

    std::function<void(GameObject*)> walk = [&](GameObject* node) {
        if (!node || count >= maxMatrices) return;
        if (auto* t = node->getTransform())
            dst[count++] = t->getGlobalMatrix().Transpose();
        for (auto* child : node->getChildren())
            walk(child);
        };
    walk(root);
    return count;
}

bool SkinningPass::init(ID3D12Device* device) {
    size_t paletteBytes = MAX_PALETTE_MATRICES * sizeof(Matrix);
    size_t morphBytes = MAX_MORPH_WEIGHTS * sizeof(float);
    size_t uploadSize = (paletteBytes + morphBytes) * FRAMES;
    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
        device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_upload));
        m_upload->SetName(L"SkinningUpload");
        m_upload->Map(0, nullptr, reinterpret_cast<void**>(&m_uploadPtr));
    }

    for (uint32_t i = 0; i < FRAMES; ++i) {
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(
                paletteBytes + morphBytes,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&m_palettes[i]));
        }
        {
            size_t outSz = MAX_SKINNED_VERTICES * STRUCT_VERTEX_BYTES;
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(
                outSz, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr,
                IID_PPV_ARGS(&m_outputs[i]));
        }
    }
    return createRootSignature(device) && createPSO(device);
}

void SkinningPass::execute(
    ID3D12GraphicsCommandList* cmd,
    uint32_t                            frameIndex,
    const std::vector<ComponentAnimation*>& animComponents)
{
    uint32_t fi = frameIndex % FRAMES;
    m_frameData[fi].meshVertexOffset.clear();
    m_frameData[fi].totalVertices = 0;
    m_frameData[fi].dispatched = false;

    if (animComponents.empty()) return;

    struct SkinJob {
        ComponentAnimation* anim = nullptr;
        Mesh* mesh = nullptr;
        uint32_t            vertexOffset = 0;
    };
    std::vector<SkinJob> jobs;
    uint32_t totalVertices = 0;

    for (ComponentAnimation* ca : animComponents) {
        if (!ca) continue;
        GameObject* go = ca->getOwner();  
        if (!go) continue;

        std::function<void(GameObject*)> gather = [&](GameObject* node) {
            if (!node) return;
            if (auto* cm = node->getComponent<ComponentMesh>()) {
                for (const auto& entry : cm->getEntries()) {
                    Mesh* mesh = entry.meshRes ? entry.meshRes->getMesh() : nullptr;
                    if (!mesh || !mesh->isSkinned()) continue;
                    if (totalVertices + mesh->getVertexCount() > MAX_SKINNED_VERTICES) {
                        LOG("SkinningPass: vertex budget exceeded, skipping mesh");
                        continue;
                    }
                    SkinJob job;
                    job.anim = ca;
                    job.mesh = mesh;
                    job.vertexOffset = totalVertices;
                    jobs.push_back(job);
                    m_frameData[fi].meshVertexOffset[mesh] = totalVertices;
                    totalVertices += mesh->getVertexCount();
                }
            }
            for (auto* child : node->getChildren()) gather(child);
            };
        gather(go);
    }

    if (jobs.empty()) return;
    m_frameData[fi].totalVertices = totalVertices;

    const size_t paletteBytes = MAX_PALETTE_MATRICES * sizeof(Matrix);
    const size_t morphBytes = MAX_MORPH_WEIGHTS * sizeof(float);
    const size_t frameSlotSize = paletteBytes + morphBytes;
    uint8_t* uploadSlot = m_uploadPtr + fi * frameSlotSize;

    auto* paletteData = reinterpret_cast<Matrix*>(uploadSlot);
    uint32_t paletteCount = 0;
    if (!animComponents.empty() && animComponents[0]) {
        GameObject* root = animComponents[0]->getOwner();
        if (root)
            paletteCount = collectJointPalette(root, paletteData, MAX_PALETTE_MATRICES);
    }
    if (paletteCount == 0) {
        paletteData[0] = Matrix::Identity.Transpose();
        paletteCount = 1;
    }

    float* morphData = reinterpret_cast<float*>(uploadSlot + paletteBytes);
    std::memset(morphData, 0, morphBytes);

    {
        auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            m_palettes[fi].Get(),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->ResourceBarrier(1, &toCopy);

        cmd->CopyBufferRegion(
            m_palettes[fi].Get(), 0,
            m_upload.Get(), fi * frameSlotSize,
            paletteBytes + morphBytes);

        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
            m_palettes[fi].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        cmd->ResourceBarrier(1, &toSRV);
    }

    {
        auto toUAV = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputs[fi].Get(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &toUAV);
    }

    cmd->SetComputeRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());

    for (const SkinJob& job : jobs) {
        Mesh* mesh = job.mesh;
        const uint32_t vertexCount = mesh->getVertexCount();

        struct SkinCB {
            uint32_t vertexCount; uint32_t vertexOffset;
            uint32_t paletteCount; uint32_t morphTargetCount;
        };
        SkinCB cb{ vertexCount, job.vertexOffset,
                   paletteCount, mesh->getMorphTargetCount() };
        cmd->SetComputeRoot32BitConstants(0, 4, &cb, 0);

        cmd->SetComputeRootShaderResourceView(1, mesh->getVertexBufferVA());

        cmd->SetComputeRootShaderResourceView(2,
            m_palettes[fi]->GetGPUVirtualAddress());

        D3D12_GPU_VIRTUAL_ADDRESS morphVA =
            mesh->hasMorphTargets() ? mesh->getMorphBufferVA() : 0;
        if (morphVA)
            cmd->SetComputeRootShaderResourceView(3, morphVA);
        else
            cmd->SetComputeRootShaderResourceView(3,
                m_palettes[fi]->GetGPUVirtualAddress()); 

        D3D12_GPU_VIRTUAL_ADDRESS weightsVA =
            m_palettes[fi]->GetGPUVirtualAddress() + paletteBytes;
        cmd->SetComputeRootShaderResourceView(4, weightsVA);
        cmd->SetComputeRootShaderResourceView(5, weightsVA); 

        cmd->SetComputeRootUnorderedAccessView(6,
            m_outputs[fi]->GetGPUVirtualAddress() +
            job.vertexOffset * STRUCT_VERTEX_BYTES);

        uint32_t groups = (vertexCount + 63) / 64;
        cmd->Dispatch(groups, 1, 1);
    }

    {
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_outputs[fi].Get());
        cmd->ResourceBarrier(1, &uavBarrier);
    }

    {
        auto toVB = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputs[fi].Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        cmd->ResourceBarrier(1, &toVB);
    }

    m_frameData[fi].dispatched = true;
}

D3D12_GPU_VIRTUAL_ADDRESS SkinningPass::getOutputVA(
    uint32_t frameIndex, uint32_t byteOffset) const
{
    return m_outputs[frameIndex % FRAMES]->GetGPUVirtualAddress() + byteOffset;
}

uint32_t SkinningPass::getMeshVertexOffset(
    const Mesh* mesh, uint32_t frameIndex) const
{
    const auto& fd = m_frameData[frameIndex % FRAMES];
    auto it = fd.meshVertexOffset.find(mesh);
    return (it != fd.meshVertexOffset.end()) ? it->second : UINT32_MAX;
}

bool SkinningPass::hasSkinnedMeshes(uint32_t frameIndex) const {
    return m_frameData[frameIndex % FRAMES].dispatched;
}

bool SkinningPass::createRootSignature(ID3D12Device* device) {
    CD3DX12_ROOT_PARAMETER params[7] = {};
    params[0].InitAsConstants(4, 0);               // b0: SkinCB
    params[1].InitAsShaderResourceView(0);         // t0: input vertices
    params[2].InitAsShaderResourceView(1);         // t1: joint palette
    params[3].InitAsShaderResourceView(2);         // t2: morph deltas
    params[4].InitAsShaderResourceView(3);         // t3: morph weights
    params[5].InitAsShaderResourceView(4);         // t4: (reserved)
    params[6].InitAsUnorderedAccessView(0);        // u0: output vertices

    CD3DX12_ROOT_SIGNATURE_DESC desc(7, params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) LOG("SkinningPass: root sig error: %s",
            (char*)error->GetBufferPointer());
        return false;
    }
    hr = device->CreateRootSignature(0,
        blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)) { LOG("SkinningPass: CreateRootSignature failed"); return false; }
    m_rootSig->SetName(L"SkinningRootSig");
    return true;
}

bool SkinningPass::createPSO(ID3D12Device* device)
{
    std::vector<uint8_t> shaderBytes;
    try {
        shaderBytes = DX::ReadData(L"skinning.cso");
    }
    catch (const std::exception& e) {
        LOG("SkinningPass: failed to load skinning.cso (%s)", e.what());
        return false;
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSig.Get();
    psoDesc.CS = { shaderBytes.data(), shaderBytes.size() };
    HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)) { LOG("SkinningPass: CreateComputePipelineState failed"); return false; }
    m_pso->SetName(L"SkinningPSO");
    return true;
}