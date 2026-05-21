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
        m_paletteNormals[i].Reset();
        m_outputs[i].Reset();
    }
    m_rootSig.Reset();
    m_pso.Reset();
}

bool SkinningPass::createBuffers(ID3D12Device* device) {
    // Upload ring: 3 sections of MAX_TOTAL_JOINTS matrices for palette,
    // followed by 3 sections for paletteNormal, all persistently mapped.
    {
        const UINT64 sz = UINT64(FRAMES_IN_FLIGHT) * MAX_TOTAL_JOINTS * sizeof(Matrix) * 2;
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

        // PaletteNormal: default heap, starts in COPY_DEST.
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(paletteSz);
            HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_paletteNormals[i]));
            if (FAILED(hr)) { LOG("SkinningPass: paletteNormal[%d] failed 0x%08X", i, hr); return false; }
            wchar_t name[40]; swprintf_s(name, L"SkinPaletteNormal[%d]", i);
            m_paletteNormals[i]->SetName(name);
            m_paletteNormalStates[i] = D3D12_RESOURCE_STATE_COPY_DEST;
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
    //   [0] b0 : root constants  4 x uint32  (numVertices, paletteOffset, vertexOffset, pad)
    //   [1] t0 : root SRV        StructuredBuffer<float4x4>   (palette)
    //   [2] t1 : root SRV        StructuredBuffer<float4x4>   (paletteNormal)
    //   [3] t2 : root SRV        StructuredBuffer<Vertex>     (input vertices)
    //   [4] t3 : root SRV        StructuredBuffer<BoneWeight> (bone weights)
    //   [5] u0 : root UAV        RWStructuredBuffer<Vertex>   (output)
    {
        CD3DX12_ROOT_PARAMETER params[6] = {};
        params[0].InitAsConstants(4, 0);              // b0
        params[1].InitAsShaderResourceView(0);        // t0 palette
        params[2].InitAsShaderResourceView(1);        // t1 paletteNormal
        params[3].InitAsShaderResourceView(2);        // t2 inVertex
        params[4].InitAsShaderResourceView(3);        // t3 boneWeights
        params[5].InitAsUnorderedAccessView(0);       // u0 outVertex

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
        auto cs = DX::ReadData(L"Skinning.cso");
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

    // Upload ring layout: first half = palette sections, second half = paletteNormal sections.
    const UINT64 sectionBytes = UINT64(MAX_TOTAL_JOINTS) * sizeof(Matrix);
    const UINT64 paletteUploadOff       = UINT64(frameIndex) * sectionBytes;
    const UINT64 paletteNormalUploadOff = UINT64(FRAMES_IN_FLIGHT) * sectionBytes
                                        + UINT64(frameIndex) * sectionBytes;

    uint8_t* paletteDst       = m_uploadMapped + paletteUploadOff;
    uint8_t* paletteNormalDst = m_uploadMapped + paletteNormalUploadOff;

    // ---- 1. Build matrix palettes on CPU ----
    // Summary log each frame so we can verify the dispatch is receiving the right mesh.
    LOG("Dispatch: jobs=%u firstJobVerts=%u firstJobJoints=%u",
        (unsigned)jobs.size(),
        jobs[0].mesh ? jobs[0].mesh->getVertexCount() : 0u,
        jobs[0].skin ? (unsigned)jobs[0].skin->jointNodeIndices.size() : 0u);

    // Joint[0] detail log every 60 frames to verify palette is not all-identity.
    // If jointWorld.t matches invBind.t exactly and palette.t == (0,0,0), the
    // world transforms are not being updated (still returning bind-pose values).
    {
        static UINT s_logFrame = 0;
        if (++s_logFrame >= 60) {
            s_logFrame = 0;
            const auto& j0 = jobs[0];
            if (!j0.skin->inverseBindMatrices.empty() && !j0.jointWorldMatrices.empty()) {
                const Matrix& ibm = j0.skin->inverseBindMatrices[0];
                const Matrix& jw  = j0.jointWorldMatrices[0];
                Matrix palette0   = ibm * jw;
                Vector3 ibmT = ibm.Translation(), jwT = jw.Translation(), palT = palette0.Translation();
                LOG("SkinPalette job[0] joint[0]: invBind.t=(%.3f,%.3f,%.3f)  jointWorld.t=(%.3f,%.3f,%.3f)  palette.t=(%.3f,%.3f,%.3f)",
                    ibmT.x, ibmT.y, ibmT.z, jwT.x, jwT.y, jwT.z, palT.x, palT.y, palT.z);
            }
        }
    }

    for (const auto& job : jobs) {
        const uint32_t jointCount = static_cast<uint32_t>(job.skin->jointNodeIndices.size());
        for (uint32_t j = 0; j < jointCount; ++j) {
            Matrix m = job.skin->inverseBindMatrices[j] * job.jointWorldMatrices[j];

            // The GBuffer shader uses mul(v, M_hlsl) where HLSL reads the uploaded bytes
            // as column-major (i.e. M_hlsl = uploaded^T).  Every other matrix in the
            // engine is transposed before upload so HLSL sees the correct value.
            // Transpose here so HLSL computes v * (invBind*jointWorld) as intended.
            Matrix mT = m.Transpose();
            memcpy(paletteDst + (job.paletteOffset + j) * sizeof(Matrix), &mT, sizeof(Matrix));

            // Normal palette: HLSL must see (m^-1)^T so we upload m^-1 and let HLSL
            // transpose it on read.  Previously inv.Transpose() was stored, making HLSL
            // see m^-1 instead of (m^-1)^T — normals were transformed incorrectly.
            Matrix inv;
            m.Invert(inv);
            memcpy(paletteNormalDst + (job.paletteOffset + j) * sizeof(Matrix),
                   &inv, sizeof(Matrix));
        }
    }

    // ---- 2. Copy upload sections → GPU buffers ----
    auto transitionTo = [&](ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES& state,
                             D3D12_RESOURCE_STATES newState) {
        if (state != newState) {
            auto b = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), state, newState);
            cmd->ResourceBarrier(1, &b);
            state = newState;
        }
    };

    // Palette
    transitionTo(m_palettes[frameIndex], m_paletteStates[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyBufferRegion(m_palettes[frameIndex].Get(), 0, m_upload.Get(), paletteUploadOff, sectionBytes);
    transitionTo(m_palettes[frameIndex], m_paletteStates[frameIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // PaletteNormal
    transitionTo(m_paletteNormals[frameIndex], m_paletteNormalStates[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyBufferRegion(m_paletteNormals[frameIndex].Get(), 0, m_upload.Get(), paletteNormalUploadOff, sectionBytes);
    transitionTo(m_paletteNormals[frameIndex], m_paletteNormalStates[frameIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

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

    // Output UAV is shared for all jobs — per-mesh offset handled via g_vertexOffset.
    cmd->SetComputeRootUnorderedAccessView(5, m_outputs[frameIndex]->GetGPUVirtualAddress());

    LOG("SkinningPass: frame=%u dispatching %u job(s), vertex budget used: %u / %u",
        frameIndex, (unsigned)jobs.size(),
        jobs.empty() ? 0u : (jobs.back().vertexOffset + (jobs.back().mesh ? jobs.back().mesh->getVertexCount() : 0u)),
        MAX_TOTAL_VERTICES);

    for (const auto& job : jobs) {
        if (!job.mesh) continue;
        if (!job.mesh->hasBoneWeights()) {
            LOG("SkinningPass: skip job — mesh has no bone weight data (CPU or GPU)");
            continue;
        }

        const uint32_t vertexCount = job.mesh->getVertexCount();
        if (vertexCount == 0) continue;

        if (job.vertexOffset + vertexCount > MAX_TOTAL_VERTICES) {
            LOG("SkinningPass: skip job — vertex range [%u, %u) exceeds output buffer size %u",
                job.vertexOffset, job.vertexOffset + vertexCount, MAX_TOTAL_VERTICES);
            continue;
        }

        const D3D12_GPU_VIRTUAL_ADDRESS vertexVA = job.mesh->getVertexBufferVA();
        const D3D12_GPU_VIRTUAL_ADDRESS bwVA     = job.mesh->getBoneWeightBufferVA();

        LOG("SkinJob: vertOffset=%u palOff=%u verts=%u bwVA=0x%llX vertVA=0x%llX",
            job.vertexOffset, job.paletteOffset, vertexCount,
            (unsigned long long)bwVA, (unsigned long long)vertexVA);

        if (vertexVA == 0 || bwVA == 0) {
            LOG("SkinningPass: skip job — mesh GPU buffers not ready (vertexVA=%llu bwVA=%llu)", vertexVA, bwVA);
            continue;
        }

        // Root constants: numVertices, paletteOffset, vertexOffset, pad.
        const uint32_t constants[4] = { vertexCount, job.paletteOffset, job.vertexOffset, 0u };
        cmd->SetComputeRoot32BitConstants(0, 4, constants, 0);

        // Palette SRVs start at the buffer base — the shader uses g_paletteOffset to index
        // into the correct skin's section.  Do NOT pre-offset the VA here; that would cause
        // the shader's own g_paletteOffset indexing to double-count the offset.
        cmd->SetComputeRootShaderResourceView(1, m_palettes[frameIndex]->GetGPUVirtualAddress());
        cmd->SetComputeRootShaderResourceView(2, m_paletteNormals[frameIndex]->GetGPUVirtualAddress());
        cmd->SetComputeRootShaderResourceView(3, vertexVA);
        cmd->SetComputeRootShaderResourceView(4, bwVA);

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
