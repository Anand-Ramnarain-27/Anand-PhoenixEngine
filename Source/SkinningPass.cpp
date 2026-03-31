#include "Globals.h"
#include "SkinningPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ComponentAnimation.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "Mesh.h"
#include "ComponentTransform.h"
#include "ReadData.h"
#include <d3dx12.h>

static constexpr uint32_t STRUCT_VERTEX_BYTES = sizeof(Mesh::Vertex);

bool SkinningPass::init(ID3D12Device* device) {
    // ?? Upload staging buffer (CPU-visible) ??????????????????????????
    size_t uploadSize = (MAX_PALETTE_MATRICES * sizeof(Matrix) * 2  // model+normal
        + MAX_MORPH_WEIGHTS * sizeof(float)) * FRAMES;
    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
        device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_upload));
        m_upload->SetName(L"SkinningUpload");
        m_upload->Map(0, nullptr, reinterpret_cast<void**>(&m_uploadPtr));
    }
    // ?? Per-frame palette + output buffers ???????????????????????????
    for (uint32_t i = 0; i < FRAMES; ++i) {
        size_t palSz = MAX_PALETTE_MATRICES * sizeof(Matrix) * 2
            + MAX_MORPH_WEIGHTS * sizeof(float);
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(palSz,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_palettes[i]));
        }
        size_t outSz = MAX_SKINNED_VERTICES * STRUCT_VERTEX_BYTES;
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(outSz,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr,
                IID_PPV_ARGS(&m_outputs[i]));
        }
    }
    return createRootSignature(device) && createPSO(device);
}

void SkinningPass::execute(
    ID3D12GraphicsCommandList* cmd,
    uint32_t frameIndex,
    const std::vector<ComponentAnimation*>& animComponents)
{
    if (animComponents.empty()) return;
    uint32_t fi = frameIndex % FRAMES;

    // Slide 33: transition output buffer to UAV for writing
    auto toUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputs[fi].Get(),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->ResourceBarrier(1, &toUAV);

    cmd->SetComputeRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());

    uint32_t vertexOffset = 0;
    uint32_t paletteOffset = 0;  // in matrices

    for (ComponentAnimation* ca : animComponents) {
        if (!ca) continue;
        // For each skinned mesh owned by this animation component,
        // build the matrix palette and dispatch.
        // (Abbreviated — full implementation iterates ca->owner subtree
        //  finding ComponentMesh entries with isSkinned())
    }

    // Slide 33: transition back to vertex buffer for rendering
    auto toVB = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputs[fi].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmd->ResourceBarrier(1, &toVB);
}

D3D12_GPU_VIRTUAL_ADDRESS SkinningPass::getOutputVA(
    uint32_t frameIndex, uint32_t byteOffset) const {
    return m_outputs[frameIndex % FRAMES]->GetGPUVirtualAddress() + byteOffset;
}

bool SkinningPass::createRootSignature(ID3D12Device* device)
{
    // [0] 4x 32-bit constants : numVertices, numJoints, numMorphTargets, pad  (b0)
    // [1] SRV paletteModel    (t0)
    // [2] SRV paletteNormal   (t1)
    // [3] SRV inVertex        (t2)
    // [4] SRV morphVertices   (t3)
    // [5] SRV morphWeights    (t4)
    // [6] UAV outVertex       (u0)
    CD3DX12_ROOT_PARAMETER params[7] = {};
    params[0].InitAsConstants(4, 0);               // b0
    params[1].InitAsShaderResourceView(0);         // t0
    params[2].InitAsShaderResourceView(1);         // t1
    params[3].InitAsShaderResourceView(2);         // t2
    params[4].InitAsShaderResourceView(3);         // t3
    params[5].InitAsShaderResourceView(4);         // t4
    params[6].InitAsUnorderedAccessView(0);        // u0

    CD3DX12_ROOT_SIGNATURE_DESC desc(7, params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) LOG("SkinningPass: root sig serialize error: %s",
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
        LOG("SkinningPass: failed to load skinning.cso (%s) - "
            "make sure it is compiled and copied to the output directory", e.what());
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
