#pragma once
#include "Globals.h"
#include "MeshEntry.h"
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class ComponentMesh;
class ResourceSkin;
class GameObject;

struct SkinInstance {
    ComponentMesh* mesh = nullptr;
    ResourceSkin* skin = nullptr;
    std::vector<Matrix>  paletteModel; 
    std::vector<Matrix>  paletteNormal;
    std::vector<float>   morphWeights; 
    uint32_t             numVertices = 0;
    uint32_t             outputOffset = 0;
};

class SkinningPass {
public:
    SkinningPass() = default;
    ~SkinningPass() = default;

    bool init(ID3D12Device* device);

    void dispatch(ID3D12GraphicsCommandList* cmd, std::vector<SkinInstance>& instances, uint32_t backBufferIdx);

    D3D12_GPU_VIRTUAL_ADDRESS getSkinnedVA(uint32_t i, uint32_t bbIdx) const;

private:
    bool createRootSignature(ID3D12Device* d);
    bool createPSO(ID3D12Device* d);
    bool createBuffers(ID3D12Device* d);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    static constexpr uint32_t FIF = 3; 
    static constexpr size_t MAX_SKINNED_VERTS = 1024 * 1024; 
    static constexpr size_t MAX_PALETTE_MATS = 512; 

    ComPtr<ID3D12Resource> m_upload[FIF];
    uint8_t* m_uploadPtr[FIF] = {};

    ComPtr<ID3D12Resource> m_output[FIF];

    std::vector<uint32_t>  m_instanceOffsets; 
};
