#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

class Mesh;
struct MeshEntry;
class ComponentAnimation;
class ComponentMesh;
class GameObject;

class SkinningPass {
public:
    static constexpr uint32_t MAX_SKINNED_VERTICES = 500000;
    static constexpr uint32_t MAX_PALETTE_MATRICES = 1024;
    static constexpr uint32_t MAX_MORPH_WEIGHTS = 2048;

    bool init(ID3D12Device* device);

    void execute(ID3D12GraphicsCommandList* cmd, uint32_t frameIndex, const std::vector<ComponentAnimation*>& animComponents);

    D3D12_GPU_VIRTUAL_ADDRESS getOutputVA(uint32_t frameIndex, uint32_t byteOffset) const;

    uint32_t getMeshVertexOffset(const Mesh* mesh, uint32_t frameIndex) const;

    bool hasSkinnedMeshes(uint32_t frameIndex) const;

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    uint32_t buildJointPalette(ComponentAnimation* ca, Matrix* dst, uint32_t maxMatrices) const;

    ComPtr<ID3D12Resource> m_upload;
    uint8_t* m_uploadPtr = nullptr;

    static constexpr uint32_t FRAMES = 3;
    ComPtr<ID3D12Resource> m_palettes[FRAMES];
    ComPtr<ID3D12Resource> m_outputs[FRAMES];

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    struct FrameData {
        std::unordered_map<const Mesh*, uint32_t> meshVertexOffset;
        uint32_t totalVertices = 0;
        bool     dispatched = false;
    };
    FrameData m_frameData[FRAMES];
};