#include "Globals.h"
#include "SkinningPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <cstring>

bool SkinningPass::init(ID3D12Device* device){
    return createBuffers(device) && createPipeline(device);
}

void SkinningPass::cleanUp(){
    m_upload.Reset();
    m_uploadMapped = nullptr;
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i){
        m_palettes[i].Reset();
        m_paletteNormals[i].Reset();
        m_outputs[i].Reset();
    }
    m_dummyBuffer.Reset();
    m_rootSig.Reset();
    m_pso.Reset();
}

bool SkinningPass::createBuffers(ID3D12Device* device){
    const UINT64 jointSz = UINT64(MAX_TOTAL_JOINTS) * sizeof(Matrix);
    const UINT64 morphWeightSz = UINT64(MAX_TOTAL_MORPH_WEIGHTS) * sizeof(float);

    {
        const UINT64 sz = UINT64(FRAMES_IN_FLIGHT) * (jointSz * 2 + morphWeightSz);
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(sz);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_upload));
        if (FAILED(hr)){ LOG("SkinningPass: upload buffer failed 0x%08X", hr); return false; }
        m_upload->SetName(L"SkinningUpload");
        m_upload->Map(0, nullptr, reinterpret_cast<void**>(&m_uploadMapped));
    }

    const UINT64 paletteSz = jointSz + morphWeightSz;
    const UINT64 outputSz = UINT64(MAX_TOTAL_VERTICES) * sizeof(Mesh::Vertex);

    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i){
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(paletteSz);
            HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_palettes[i]));
            if (FAILED(hr)){ LOG("SkinningPass: palette[%d] failed 0x%08X", i, hr); return false; }
            wchar_t name[32]; swprintf_s(name, L"SkinPalette[%d]", i);
            m_palettes[i]->SetName(name);
            m_paletteStates[i] = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(paletteSz);
            HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_paletteNormals[i]));
            if (FAILED(hr)){ LOG("SkinningPass: paletteNormal[%d] failed 0x%08X", i, hr); return false; }
            wchar_t name[40]; swprintf_s(name, L"SkinPaletteNormal[%d]", i);
            m_paletteNormals[i]->SetName(name);
            m_paletteNormalStates[i] = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(outputSz,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr, IID_PPV_ARGS(&m_outputs[i]));
            if (FAILED(hr)){ LOG("SkinningPass: output[%d] failed 0x%08X", i, hr); return false; }
            wchar_t name[32]; swprintf_s(name, L"SkinOutput[%d]", i);
            m_outputs[i]->SetName(name);
        }
    }

    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(256);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dummyBuffer));
        if (FAILED(hr)){ LOG("SkinningPass: dummy buffer failed 0x%08X", hr); return false; }
        m_dummyBuffer->SetName(L"SkinDummy");
    }

    return true;
}

bool SkinningPass::createPipeline(ID3D12Device* device){
    {
        CD3DX12_ROOT_PARAMETER params[8] = {};
        params[0].InitAsConstants(5, 0);
        params[1].InitAsShaderResourceView(0);
        params[2].InitAsShaderResourceView(1);
        params[3].InitAsShaderResourceView(2);
        params[4].InitAsShaderResourceView(3);
        params[5].InitAsUnorderedAccessView(0);
        params[6].InitAsShaderResourceView(4);
        params[7].InitAsShaderResourceView(5);

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        ComPtr<ID3DBlob> blob, error;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &blob, &error);
        if (FAILED(hr)){
            if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
            LOG("SkinningPass: SerializeRootSignature failed 0x%08X", hr);
            return false;
        }
        hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSig));
        if (FAILED(hr)){ LOG("SkinningPass: CreateRootSignature failed 0x%08X", hr); return false; }
    }

    {
        auto cs = DX::ReadData(L"Skinning.cso");
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m_rootSig.Get();
        desc.CS = { cs.data(), cs.size() };
        HRESULT hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso));
        if (FAILED(hr)){
            LOG("SkinningPass: CreateComputePipelineState failed 0x%08X", hr);
            return false;
        }
    }
    return true;
}

void SkinningPass::dispatch(ID3D12GraphicsCommandList* cmd,
                             const std::vector<SkinJob>& jobs,
                             UINT frameIndex){
    if (jobs.empty()) return;

    BEGIN_EVENT(cmd, "SkinningPass");

    const UINT64 jointSz = UINT64(MAX_TOTAL_JOINTS) * sizeof(Matrix);
    const UINT64 morphWeightSz = UINT64(MAX_TOTAL_MORPH_WEIGHTS) * sizeof(float);
    const UINT64 paletteUploadOff = UINT64(frameIndex) * jointSz;
    const UINT64 paletteNormalUploadOff = UINT64(FRAMES_IN_FLIGHT) * jointSz + UINT64(frameIndex) * jointSz;
    const UINT64 morphWeightUploadOff = UINT64(FRAMES_IN_FLIGHT) * jointSz * 2 + UINT64(frameIndex) * morphWeightSz;

    uint8_t* paletteDst = m_uploadMapped + paletteUploadOff;
    uint8_t* paletteNormalDst = m_uploadMapped + paletteNormalUploadOff;
    uint8_t* morphWeightDst = m_uploadMapped + morphWeightUploadOff;

    for (const auto& job : jobs){
        if (job.skin){
            const uint32_t jointCount = static_cast<uint32_t>(job.skin->jointNodeIndices.size());
            for (uint32_t j = 0; j < jointCount; ++j){
                Matrix m = job.skin->inverseBindMatrices[j] * job.jointWorldMatrices[j] * job.meshWorldInverse;

#ifdef _DEBUG
                if (j == 0 && job.paletteOffset == 0){
                    static bool s_tposeLogged = false;
                    if (!s_tposeLogged){
                        s_tposeLogged = true;
                        LOG("[TposeCheck] Palette[0] r0: %.3f %.3f %.3f %.3f  r3: %.3f %.3f %.3f %.3f",
                            m._11, m._12, m._13, m._14,
                            m._41, m._42, m._43, m._44);
                    }
                }
#endif

                Matrix mUpload = m.Transpose();
                memcpy(paletteDst + (job.paletteOffset + j) * sizeof(Matrix), &mUpload, sizeof(Matrix));

                Matrix inv;
                m.Invert(inv);
                memcpy(paletteNormalDst + (job.paletteOffset + j) * sizeof(Matrix),
                       &inv, sizeof(Matrix));
            }
        }
        if (!job.morphWeights.empty()){
            memcpy(morphWeightDst + job.morphWeightOffset * sizeof(float),
                   job.morphWeights.data(),
                   job.morphWeights.size() * sizeof(float));
        }
    }

    auto transitionTo = [&](ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES& state,
                             D3D12_RESOURCE_STATES newState){
        if (state != newState){
            auto b = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), state, newState);
            cmd->ResourceBarrier(1, &b);
            state = newState;
        }
    };

    transitionTo(m_palettes[frameIndex], m_paletteStates[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyBufferRegion(m_palettes[frameIndex].Get(), 0, m_upload.Get(), paletteUploadOff, jointSz);
    cmd->CopyBufferRegion(m_palettes[frameIndex].Get(), jointSz, m_upload.Get(), morphWeightUploadOff, morphWeightSz);
    transitionTo(m_palettes[frameIndex], m_paletteStates[frameIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    transitionTo(m_paletteNormals[frameIndex], m_paletteNormalStates[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyBufferRegion(m_paletteNormals[frameIndex].Get(), 0, m_upload.Get(), paletteNormalUploadOff, jointSz);
    transitionTo(m_paletteNormals[frameIndex], m_paletteNormalStates[frameIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputs[frameIndex].Get(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &b);
    }

    cmd->SetComputeRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());

    cmd->SetComputeRootUnorderedAccessView(5, m_outputs[frameIndex]->GetGPUVirtualAddress());

    const D3D12_GPU_VIRTUAL_ADDRESS dummyVA = m_dummyBuffer->GetGPUVirtualAddress();

    for (const auto& job : jobs){
        if (!job.mesh) continue;

        const bool hasSkin = (job.skin != nullptr) && !job.jointWorldMatrices.empty();
        const bool hasMorph = job.mesh->hasMorphTargets() && !job.morphWeights.empty();
        if (!hasSkin && !hasMorph) continue;

        const uint32_t vertexCount = job.mesh->getVertexCount();
        const uint32_t numMorphTargets = hasMorph ? static_cast<uint32_t>(job.morphWeights.size()) : 0u;
        const uint32_t numJoints = hasSkin ? static_cast<uint32_t>(job.skin->jointNodeIndices.size()) : 0u;
        if (vertexCount == 0) continue;

        const D3D12_GPU_VIRTUAL_ADDRESS vertexVA = job.mesh->getVertexBufferVA();
        if (vertexVA == 0) continue;

        const D3D12_GPU_VIRTUAL_ADDRESS rawBwVA = job.mesh->getBoneWeightBufferVA();
        if (hasSkin && rawBwVA == 0) continue;
        const D3D12_GPU_VIRTUAL_ADDRESS bwVA = hasSkin ? rawBwVA : dummyVA;

        const D3D12_GPU_VIRTUAL_ADDRESS morphVtxRaw = hasMorph ? job.mesh->getMorphTargetBufferVA() : 0;
        const bool validMorph = hasMorph && (morphVtxRaw != 0);
        if (hasMorph && !validMorph)
            LOG("SkinningPass: getMorphTargetBufferVA()==0 — morph disabled this frame (buffer still uploading?)");
        const D3D12_GPU_VIRTUAL_ADDRESS morphVtxVA = validMorph ? morphVtxRaw : dummyVA;
        const D3D12_GPU_VIRTUAL_ADDRESS morphWgtVA = validMorph
            ? m_palettes[frameIndex]->GetGPUVirtualAddress() + jointSz + UINT64(job.morphWeightOffset) * sizeof(float)
            : dummyVA;
        const uint32_t effectiveMorphTargets = validMorph ? numMorphTargets : 0u;

        const uint32_t constants[5] = { vertexCount, job.paletteOffset, job.vertexOffset, effectiveMorphTargets, numJoints };
        cmd->SetComputeRoot32BitConstants(0, 5, constants, 0);

        const UINT64 paletteJointOff = UINT64(job.paletteOffset) * sizeof(Matrix);
        cmd->SetComputeRootShaderResourceView(1, m_palettes[frameIndex]->GetGPUVirtualAddress() + paletteJointOff);
        cmd->SetComputeRootShaderResourceView(2, m_paletteNormals[frameIndex]->GetGPUVirtualAddress() + paletteJointOff);
        cmd->SetComputeRootShaderResourceView(3, vertexVA);
        cmd->SetComputeRootShaderResourceView(4, bwVA);
        cmd->SetComputeRootShaderResourceView(6, morphVtxVA);
        cmd->SetComputeRootShaderResourceView(7, morphWgtVA);

        const UINT groups = (vertexCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
        cmd->Dispatch(groups, 1, 1);
    }

    {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputs[frameIndex].Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        cmd->ResourceBarrier(1, &b);
    }

    END_EVENT(cmd);
}
