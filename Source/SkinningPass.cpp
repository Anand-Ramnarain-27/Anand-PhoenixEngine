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
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
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

    // Upload ring: FRAMES_IN_FLIGHT sections for palette, then paletteNormal, then morph weights.
    {
        const UINT64 sz = UINT64(FRAMES_IN_FLIGHT) * (jointSz * 2 + morphWeightSz);
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(sz);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_upload));
        if (FAILED(hr)) { LOG("SkinningPass: upload buffer failed 0x%08X", hr); return false; }
        m_upload->SetName(L"SkinningUpload");
        m_upload->Map(0, nullptr, reinterpret_cast<void**>(&m_uploadMapped));
    }

    // palette GPU buffer = joint matrices + morph weights in one allocation.
    const UINT64 paletteSz = jointSz + morphWeightSz;
    const UINT64 outputSz = UINT64(MAX_TOTAL_VERTICES) * sizeof(Mesh::Vertex);

    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        // Palette+weights: default heap, starts in COPY_DEST.
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

    // Dummy buffer: bound to unused SRV slots (bone-weight or morph slots on jobs that don't
    // need them).  Buffers in COMMON state auto-promote to any read state, so no transition needed.
    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(256);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dummyBuffer));
        if (FAILED(hr)) { LOG("SkinningPass: dummy buffer failed 0x%08X", hr); return false; }
        m_dummyBuffer->SetName(L"SkinDummy");
    }

    return true;
}

bool SkinningPass::createPipeline(ID3D12Device* device){
    // Root signature (no descriptor heap needed — all root descriptors):
    //   [0] b0 : root constants  5 x uint32  (numVertices, paletteOffset, vertexOffset, numMorphTargets, numJoints)
    //   [1] t0 : root SRV        StructuredBuffer<float4x4>    (palette)
    //   [2] t1 : root SRV        StructuredBuffer<float4x4>    (paletteNormal)
    //   [3] t2 : root SRV        StructuredBuffer<Vertex>      (input vertices)
    //   [4] t3 : root SRV        StructuredBuffer<BoneWeight>  (bone weights)
    //   [5] u0 : root UAV        RWStructuredBuffer<Vertex>    (output)
    //   [6] t4 : root SRV        StructuredBuffer<MorphVertex> (morph target deltas)
    //   [7] t5 : root SRV        StructuredBuffer<float>       (morph blend weights)
    {
        CD3DX12_ROOT_PARAMETER params[8] = {};
        params[0].InitAsConstants(5, 0); // b0 — 5 DWORDs
        params[1].InitAsShaderResourceView(0); // t0 palette
        params[2].InitAsShaderResourceView(1); // t1 paletteNormal
        params[3].InitAsShaderResourceView(2); // t2 inVertex
        params[4].InitAsShaderResourceView(3); // t3 boneWeights
        params[5].InitAsUnorderedAccessView(0); // u0 outVertex
        params[6].InitAsShaderResourceView(4); // t4 morphVertices
        params[7].InitAsShaderResourceView(5); // t5 morphWeights

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
                             UINT frameIndex){
    if (jobs.empty()) return;

    BEGIN_EVENT(cmd, "SkinningPass");

    // Upload ring offsets (see header comment for layout).
    const UINT64 jointSz = UINT64(MAX_TOTAL_JOINTS) * sizeof(Matrix);
    const UINT64 morphWeightSz = UINT64(MAX_TOTAL_MORPH_WEIGHTS) * sizeof(float);
    const UINT64 paletteUploadOff = UINT64(frameIndex) * jointSz;
    const UINT64 paletteNormalUploadOff = UINT64(FRAMES_IN_FLIGHT) * jointSz + UINT64(frameIndex) * jointSz;
    const UINT64 morphWeightUploadOff = UINT64(FRAMES_IN_FLIGHT) * jointSz * 2 + UINT64(frameIndex) * morphWeightSz;

    uint8_t* paletteDst = m_uploadMapped + paletteUploadOff;
    uint8_t* paletteNormalDst = m_uploadMapped + paletteNormalUploadOff;
    uint8_t* morphWeightDst = m_uploadMapped + morphWeightUploadOff;

    // ---- 1. Build matrix palettes + morph weights on CPU ----
    for (const auto& job : jobs) {
        // Joint matrices (skip morph-only jobs that have no skin).
        if (job.skin) {
            const uint32_t jointCount = static_cast<uint32_t>(job.skin->jointNodeIndices.size());
            for (uint32_t j = 0; j < jointCount; ++j) {
                // palette[j] = IBP[j] * jointWorld[j] * meshWorldInverse
                // This produces deformation in mesh-local space.  The render worldMatrix
                // (= mesh node's world transform) then places the result in world space.
                // At T-pose: IBP * jointBind * meshWorldInverse = I regardless of placement.
                Matrix m = job.skin->inverseBindMatrices[j] * job.jointWorldMatrices[j] * job.meshWorldInverse;

#ifdef _DEBUG
                // T-pose check: for models with no armature correction Palette[0] ≈ Identity.
                // For models with an armature correction (e.g. Blender Z-up) Palette[0] ≈ correction matrix.
                if (j == 0 && job.paletteOffset == 0) {
                    static bool s_tposeLogged = false;
                    if (!s_tposeLogged) {
                        s_tposeLogged = true;
                        LOG("[TposeCheck] Palette[0] r0: %.3f %.3f %.3f %.3f  r3: %.3f %.3f %.3f %.3f",
                            m._11, m._12, m._13, m._14,
                            m._41, m._42, m._43, m._44);
                    }
                }
#endif

                // Engine-wide convention: row-major DX matrices are uploaded TRANSPOSED so the
                // shader (HLSL packs StructuredBuffer<float4x4> column-major by default) reads
                // them back correctly for mul(vector, matrix). Every other pass does this
                // (GBufferPass/MeshRenderPass `.Transpose()` on upload; GBufferVS `mul(v, M)`).
                // Without it each joint matrix is read transposed — rotations invert and the
                // translation lands in the wrong place — giving an inverted/deformed skinned
                // mesh that still animates. The Skinning.hlsl mul order matches GBufferVS.
                Matrix mUpload = m.Transpose();
                memcpy(paletteDst + (job.paletteOffset + j) * sizeof(Matrix), &mUpload, sizeof(Matrix));

                // Normal matrix = inverse-transpose of m. Upload it transposed too (which is
                // just m's inverse) so the shader reads back (m^-1)^T for mul(normal, M).
                Matrix inv;
                m.Invert(inv);
                memcpy(paletteNormalDst + (job.paletteOffset + j) * sizeof(Matrix),
                       &inv, sizeof(Matrix));
            }
        }
        // Morph weights — written at job.morphWeightOffset within the combined weight section.
        if (!job.morphWeights.empty()) {
            memcpy(morphWeightDst + job.morphWeightOffset * sizeof(float),
                   job.morphWeights.data(),
                   job.morphWeights.size() * sizeof(float));
        }
    }

    // ---- 2. Copy upload sections → GPU buffers ----
    auto transitionTo = [&](ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES& state,
                             D3D12_RESOURCE_STATES newState){
        if (state != newState) {
            auto b = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), state, newState);
            cmd->ResourceBarrier(1, &b);
            state = newState;
        }
    };

    // Palette matrices + morph weights — both sections are in the same GPU buffer, so one barrier covers both.
    transitionTo(m_palettes[frameIndex], m_paletteStates[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyBufferRegion(m_palettes[frameIndex].Get(), 0, m_upload.Get(), paletteUploadOff, jointSz);
    cmd->CopyBufferRegion(m_palettes[frameIndex].Get(), jointSz, m_upload.Get(), morphWeightUploadOff, morphWeightSz);
    transitionTo(m_palettes[frameIndex], m_paletteStates[frameIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // PaletteNormal
    transitionTo(m_paletteNormals[frameIndex], m_paletteNormalStates[frameIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyBufferRegion(m_paletteNormals[frameIndex].Get(), 0, m_upload.Get(), paletteNormalUploadOff, jointSz);
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

    const D3D12_GPU_VIRTUAL_ADDRESS dummyVA = m_dummyBuffer->GetGPUVirtualAddress();

    for (const auto& job : jobs) {
        if (!job.mesh) continue;

        const bool hasSkin = (job.skin != nullptr) && !job.jointWorldMatrices.empty();
        // Morphing is active only when the mesh has targets AND the caller supplied weights.
        const bool hasMorph = job.mesh->hasMorphTargets() && !job.morphWeights.empty();
        if (!hasSkin && !hasMorph) continue;

        const uint32_t vertexCount = job.mesh->getVertexCount();
        const uint32_t numMorphTargets = hasMorph ? static_cast<uint32_t>(job.morphWeights.size()) : 0u;
        const uint32_t numJoints = hasSkin ? static_cast<uint32_t>(job.skin->jointNodeIndices.size()) : 0u;
        if (vertexCount == 0) continue;

        const D3D12_GPU_VIRTUAL_ADDRESS vertexVA = job.mesh->getVertexBufferVA();
        if (vertexVA == 0) continue;

        // Bone-weight buffer: required for skin jobs, dummy for morph-only jobs.
        const D3D12_GPU_VIRTUAL_ADDRESS rawBwVA = job.mesh->getBoneWeightBufferVA();
        if (hasSkin && rawBwVA == 0) continue; // skin job but bone weights not on GPU yet
        const D3D12_GPU_VIRTUAL_ADDRESS bwVA = hasSkin ? rawBwVA : dummyVA;

        // Morph vertex deltas: per-mesh buffer uploaded at import time.
        // Morph weights: live in the extended section of the per-frame palette buffer.
        // Guard: morph vertex buffer must be on GPU. If it isn't (upload deferred), fall back to no-morph
        // so we bind a valid VA rather than 0 which would cause a GPU page fault.
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

        // Per-job palette SRVs offset to this skin's base joint (harmless at offset 0 for morph-only jobs).
        const UINT64 paletteJointOff = UINT64(job.paletteOffset) * sizeof(Matrix);
        cmd->SetComputeRootShaderResourceView(1, m_palettes[frameIndex]->GetGPUVirtualAddress() + paletteJointOff);
        cmd->SetComputeRootShaderResourceView(2, m_paletteNormals[frameIndex]->GetGPUVirtualAddress() + paletteJointOff);
        cmd->SetComputeRootShaderResourceView(3, vertexVA);
        cmd->SetComputeRootShaderResourceView(4, bwVA);
        // [5] u0 already bound above
        cmd->SetComputeRootShaderResourceView(6, morphVtxVA);
        cmd->SetComputeRootShaderResourceView(7, morphWgtVA);

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
