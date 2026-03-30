#include "Globals.h"
#include "SkinningPass.h"
#include "ResourceSkin.h"
#include "ComponentMesh.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <algorithm>

static constexpr UINT SLOT_CONST = 0;  
static constexpr UINT SLOT_PAL_MODEL = 1; 
static constexpr UINT SLOT_PAL_NORMAL = 2;
static constexpr UINT SLOT_IN_VERTS = 3; 
static constexpr UINT SLOT_SKIN_W = 4;
static constexpr UINT SLOT_MORPH_V = 5;
static constexpr UINT SLOT_MORPH_W = 6; 
static constexpr UINT SLOT_OUT_VERTS = 7;  

bool SkinningPass::init(ID3D12Device* device) {
    return createRootSignature(device)
        && createPSO(device)
        && createBuffers(device);
}

bool SkinningPass::createRootSignature(ID3D12Device* device) {
    CD3DX12_ROOT_PARAMETER params[8];
    params[SLOT_CONST].InitAsConstants(4, 0, 0);    
    params[SLOT_PAL_MODEL].InitAsShaderResourceView(0, 0); 
    params[SLOT_PAL_NORMAL].InitAsShaderResourceView(1, 0);
    params[SLOT_IN_VERTS].InitAsShaderResourceView(2, 0);
    params[SLOT_SKIN_W].InitAsShaderResourceView(3, 0); 
    params[SLOT_MORPH_V].InitAsShaderResourceView(4, 0); 
    params[SLOT_MORPH_W].InitAsShaderResourceView(5, 0);
    params[SLOT_OUT_VERTS].InitAsUnorderedAccessView(0, 0);

    CD3DX12_ROOT_SIGNATURE_DESC desc(_countof(params), params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);
    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
        if (err) LOG("SkinRS: %s", (char*)err->GetBufferPointer());
        return false;
    }
    return SUCCEEDED(device->CreateRootSignature(0,
        blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSig)));
}

bool SkinningPass::createPSO(ID3D12Device* device) {
    auto cs = DX::ReadData(L"Skinning.cso");
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.CS = { cs.data(), cs.size() };
    return SUCCEEDED(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso)));
}

bool SkinningPass::createBuffers(ID3D12Device* device) {
    const size_t uploadSize = MAX_PALETTE_MATS * 2 * sizeof(Matrix) + MAX_PALETTE_MATS * sizeof(float);

    const size_t outputSize = MAX_SKINNED_VERTS * sizeof(Mesh::Vertex); 

    for (uint32_t i = 0; i < FIF; ++i) {
        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
            if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_upload[i])))) return false;
            m_upload[i]->SetName(L"SkinUpload");
            m_upload[i]->Map(0, nullptr, (void**)&m_uploadPtr[i]);
        }

        {
            auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            auto bd = CD3DX12_RESOURCE_DESC::Buffer(outputSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr, IID_PPV_ARGS(&m_output[i])))) 
                return false;

            m_output[i]->SetName(L"SkinOutput");
        }
    }
    return true;
}

void SkinningPass::dispatch(ID3D12GraphicsCommandList* cmd, std::vector<SkinInstance>& instances, uint32_t bbIdx) {
    if (instances.empty()) return;
    uint32_t fi = bbIdx % FIF;

    auto toUAV = CD3DX12_RESOURCE_BARRIER::Transition(m_output[fi].Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->ResourceBarrier(1, &toUAV);

    cmd->SetComputeRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());

    uint8_t* uploadBase = m_uploadPtr[fi];
    size_t   uploadOff = 0;
    uint32_t vertexBase = 0;

    m_instanceOffsets.resize(instances.size());

    for (uint32_t idx = 0; idx < (uint32_t)instances.size(); ++idx) {
        SkinInstance& si = instances[idx];
        uint32_t nv = si.numVertices;
        uint32_t nj = (uint32_t)si.paletteModel.size();
        uint32_t nm = (uint32_t)si.morphWeights.size();

        m_instanceOffsets[idx] = vertexBase;
        si.outputOffset = vertexBase;

        D3D12_GPU_VIRTUAL_ADDRESS palModelVA = m_upload[fi]->GetGPUVirtualAddress() + uploadOff;
        for (uint32_t j = 0; j < nj; ++j) {
            Matrix t = si.paletteModel[j].Transpose();
            memcpy(uploadBase + uploadOff, &t, sizeof(Matrix));
            uploadOff += sizeof(Matrix);
        }

        D3D12_GPU_VIRTUAL_ADDRESS palNormVA = m_upload[fi]->GetGPUVirtualAddress() + uploadOff;
        for (uint32_t j = 0; j < nj; ++j) {
            Matrix t = si.paletteNormal[j].Transpose();
            memcpy(uploadBase + uploadOff, &t, sizeof(Matrix));
            uploadOff += sizeof(Matrix);
        }

        D3D12_GPU_VIRTUAL_ADDRESS morphWVA = m_upload[fi]->GetGPUVirtualAddress() + uploadOff;
        if (nm > 0) {
            memcpy(uploadBase + uploadOff, si.morphWeights.data(), nm * sizeof(float));
            uploadOff += nm * sizeof(float);
            uploadOff = (uploadOff + 15) & ~15;
        }

        uint32_t consts[4] = { nv, nj, nm, 0 };
        cmd->SetComputeRoot32BitConstants(SLOT_CONST, 4, consts, 0);

        D3D12_GPU_VIRTUAL_ADDRESS bindPoseVA = si.mesh->getBindPoseVA();
        D3D12_GPU_VIRTUAL_ADDRESS skinWVA = si.mesh->getSkinWeightsVA();
        D3D12_GPU_VIRTUAL_ADDRESS morphVVA = si.mesh->getMorphVertsVA();

        cmd->SetComputeRootShaderResourceView(SLOT_PAL_MODEL, palModelVA);
        cmd->SetComputeRootShaderResourceView(SLOT_PAL_NORMAL, palNormVA);
        cmd->SetComputeRootShaderResourceView(SLOT_IN_VERTS, bindPoseVA);
        cmd->SetComputeRootShaderResourceView(SLOT_SKIN_W, skinWVA);
        cmd->SetComputeRootShaderResourceView(SLOT_MORPH_V, morphVVA);
        cmd->SetComputeRootShaderResourceView(SLOT_MORPH_W, morphWVA);

        D3D12_GPU_VIRTUAL_ADDRESS outVA = m_output[fi]->GetGPUVirtualAddress() + (size_t)vertexBase * sizeof(Mesh::Vertex); 
        cmd->SetComputeRootUnorderedAccessView(SLOT_OUT_VERTS, outVA);

        uint32_t groups = (nv + 63) / 64;
        cmd->Dispatch(groups, 1, 1);

        vertexBase += nv;
    }

    auto toVB = CD3DX12_RESOURCE_BARRIER::Transition(m_output[fi].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmd->ResourceBarrier(1, &toVB);
}

D3D12_GPU_VIRTUAL_ADDRESS SkinningPass::getSkinnedVA(
    uint32_t instanceIdx, uint32_t bbIdx) const {
    uint32_t fi = bbIdx % FIF;
    size_t offset = (size_t)m_instanceOffsets[instanceIdx] * sizeof(Mesh::Vertex);
    return m_output[fi]->GetGPUVirtualAddress() + offset;
}
