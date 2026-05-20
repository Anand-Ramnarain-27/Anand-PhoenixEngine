#include "Globals.h"
#include "SkinningPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <cstring>

bool SkinningPass::init(ID3D12Device* device) {
    return createBuffers(device) && createPipeline(device);
}

void SkinningPass::cleanUp() {
    m_upload.Reset();
    m_uploadMapped = nullptr;
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        m_palettes[i].Reset();
        m_outputs[i].Reset();
    }
    m_rootSig.Reset();
    m_pso.Reset();
}

bool SkinningPass::createBuffers(ID3D12Device* device) {
    // Upload ring: 3 sections of MAX_TOTAL_JOINTS matrices, persistently mapped.
    {
        const UINT64 sz = UINT64(FRAMES_IN_FLIGHT) * MAX_TOTAL_JOINTS * sizeof(Matrix);
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(sz);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_upload));
        if (FAILED(hr)) { LOG("SkinningPass: upload buffer failed 0x%08X", hr); return false; }
        m_upload->SetName(L"SkinningUpload");
        m_upload->Map(0, nullptr, reinterpret_cast<void**>(&m_uploadMapped));
    }

    const UINT64 paletteSz = UINT64(MAX_TOTAL_JOINTS) * sizeof(Matrix);
    const UINT64 outputSz  = UINT64(MAX_TOTAL_VERTICES) * sizeof(Mesh::Vertex);

    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        // Palette: default heap, starts in COPY_DEST.
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(paletteSz);
            HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_palettes[i]));
            if (FAILED(hr)) { LOG("SkinningPass: palette[%d] failed 0x%08X", i, hr); return false; }
            wchar_t name[32]; swprintf_s(name, L"SkinPalette[%d]", i);
            m_palettes[i]->SetName(name);
            m_paletteStates[i] = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        // Output: default heap, UAV-capable, starts as VERTEX_AND_CONSTANT_BUFFER.
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(outputSz,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr, IID_PPV_ARGS(&m_outputs[i]));
            if (FAILED(hr)) { LOG("SkinningPass: output[%d] failed 0x%08X", i, hr); return false; }
            wchar_t name[32]; swprintf_s(name, L"SkinOutput[%d]", i);
            m_outputs[i]->SetName(name);
        }
    }
    return true;
}

bool SkinningPass::createPipeline(ID3D12Device* device) {
    // Root signature (no descriptor heap needed — all root descriptors):
    //   [0] b0 : root constants  4 x uint32  (vertexCount, paletteOffset, vertexOffset, pad)
    //   [1] t0 : root SRV        StructuredBuffer<Vertex>     (input vertices)
    //   [2] t1 : root SRV        StructuredBuffer<BoneWeight> (bone weights)
    //   [3] t2 : root SRV        StructuredBuffer<float4x4>   (palette)
    //   [4] u0 : root UAV        RWStructuredBuffer<Vertex>   (output)
    {
        CD3DX12_ROOT_PARAMETER params[5] = {};
        params[0].InitAsConstants(4, 0);              // b0
        params[1].InitAsShaderResourceView(0);        // t0
        params[2].InitAsShaderResourceView(1);        // t1
        params[3].InitAsShaderResourceView(2);        // t2
        params[4].InitAsUnorderedAccessView(0);       // u0

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        ComPtr<ID3DBlob> blob, error;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &blob, &error);
        if (FAILED(hr)) {
            if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
            LOG("SkinningPass: SerializeRootSignature failed 0x%08X", hr);
            return false;
        }
        hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSig));
        if (FAILED(hr)) { LOG("SkinningPass: CreateRootSignature failed 0x%08X", hr); return false; }
    }

    // Compute PSO.
    {
        auto cs = DX::ReadData(L"SkinningCS.cso");
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m_rootSig.Get();
        desc.CS = { cs.data(), cs.size() };
        HRESULT hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso));
        if (FAILED(hr)) {
            LOG("SkinningPass: CreateComputePipelineState failed 0x%08X", hr);
            return false;
        }
    }
    return true;
}

void SkinningPass::dispatch(ID3D12GraphicsCommandList* cmd,
                             const std::vector<SkinJob>& jobs,
                             UINT frameIndex) {
    if (jobs.empty()) return;

    BEGIN_EVENT(cmd, "SkinningPass");

    // ---- 1. Build matrix palette on CPU and write into the upload ring ----
    // Section for this frame starts at frameIndex * MAX_TOTAL_JOINTS matrices.
    const UINT64 uploadSectionOff =
        UINT64(frameIndex) * MAX_TOTAL_JOINTS * sizeof(Matrix);
    uint8_t* uploadDst = m_uploadMapped + uploadSectionOff;

    for (const auto& job : jobs) {
        const uint32_t jointCount =
            static_cast<uint32_t>(job.skin->jointNodeIndices.size());
        for (uint32_t j = 0; j < jointCount; ++j) {
            // palette[j] = inverseBindMatrix[j] * jointWorldTransform[j]
            // Row-major order: invBind applied first, then world.
            Matrix m = job.skin->inverseBindMatrices[j] * job.jointWorldMatrices[j];
            memcpy(uploadDst + (job.paletteOffset + j) * sizeof(Matrix),
                   &m, sizeof(Matrix));
        }
    }

    // ---- 2. Copy upload section → palette[frame] ----
    {
        // If palette is still in COPY_DEST (first use or after previous SRV use), skip the
        // first transition; otherwise transition back from NON_PIXEL_SHADER_RESOURCE.
        if (m_paletteStates[frameIndex] != D3D12_RESOURCE_STATE_COPY_DEST) {
            auto b = CD3DX12_RESOURCE_BARRIER::Transition(
                m_palettes[frameIndex].Get(),
                m_paletteStates[frameIndex],
                D3D12_RESOURCE_STATE_COPY_DEST);
            cmd->ResourceBarrier(1, &b);
            m_paletteStates[frameIndex] = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        cmd->CopyBufferRegion(
            m_palettes[frameIndex].Get(), 0,
            m_upload.Get(), uploadSectionOff,
            UINT64(MAX_TOTAL_JOINTS) * sizeof(Matrix));

        auto b = CD3DX12_RESOURCE_BARRIER::Transition(
            m_palettes[frameIndex].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &b);
        m_paletteStates[frameIndex] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    // ---- 3. Transition output: VERTEX_AND_CONSTANT_BUFFER → UNORDERED_ACCESS ----
    {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputs[frameIndex].Get(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &b);
    }

    // ---- 4. Dispatch skinning CS for each job ----
    cmd->SetComputeRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());

    // Palette SRV is shared for all jobs this frame.
    cmd->SetComputeRootShaderResourceView(3,
        m_palettes[frameIndex]->GetGPUVirtualAddress());

    // Output UAV points to the start of the whole output buffer;
    // per-mesh offset is handled inside the shader via g_vertexOffset.
    cmd->SetComputeRootUnorderedAccessView(4,
        m_outputs[frameIndex]->GetGPUVirtualAddress());

    for (const auto& job : jobs) {
        if (!job.mesh || !job.mesh->hasBoneWeights()) continue;

        const uint32_t vertexCount = job.mesh->getVertexCount();
        if (vertexCount == 0) continue;

        const D3D12_GPU_VIRTUAL_ADDRESS vertexVA = job.mesh->getVertexBufferVA();
        const D3D12_GPU_VIRTUAL_ADDRESS bwVA     = job.mesh->getBoneWeightBufferVA();
        if (vertexVA == 0 || bwVA == 0) continue;

        // Root constants: vertexCount, paletteOffset, vertexOffset, pad.
        const uint32_t constants[4] = {
            vertexCount,
            job.paletteOffset,
            job.vertexOffset,
            0u
        };
        cmd->SetComputeRoot32BitConstants(0, 4, constants, 0);
        cmd->SetComputeRootShaderResourceView(1, vertexVA);   // t0 InputVertices
        cmd->SetComputeRootShaderResourceView(2, bwVA);       // t1 BoneWeights

        const UINT groups = (vertexCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
        cmd->Dispatch(groups, 1, 1);
    }

    // ---- 5. Transition output: UNORDERED_ACCESS → VERTEX_AND_CONSTANT_BUFFER ----
    {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputs[frameIndex].Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        cmd->ResourceBarrier(1, &b);
    }

    END_EVENT(cmd);
}
