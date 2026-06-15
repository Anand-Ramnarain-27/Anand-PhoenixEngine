#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "ResourceModel.h"
#include "Mesh.h"

using Microsoft::WRL::ComPtr;

class SkinningPass {
public:
    static constexpr uint32_t MAX_TOTAL_JOINTS = 1024;
    static constexpr uint32_t MAX_TOTAL_VERTICES = 262144;
    static constexpr uint32_t MAX_TOTAL_MORPH_WEIGHTS = 64;
    static constexpr uint32_t THREAD_GROUP_SIZE = 64;

    struct SkinJob {
        const ResourceModel::Skin* skin = nullptr;
        std::vector<Matrix> jointWorldMatrices;
        Matrix meshWorldInverse = Matrix::Identity;
        Mesh* mesh = nullptr;
        uint32_t paletteOffset = 0;
        uint32_t vertexOffset = 0;
        std::vector<float> morphWeights;
        uint32_t morphWeightOffset = 0;
    };

    bool init(ID3D12Device* device);
    void cleanUp();

    void dispatch(ID3D12GraphicsCommandList* cmd,
                  const std::vector<SkinJob>& jobs,
                  UINT frameIndex);

    ID3D12Resource* getOutputBuffer(UINT frameIndex) const { return m_outputs[frameIndex].Get(); }

private:
    bool createBuffers(ID3D12Device* device);
    bool createPipeline(ID3D12Device* device);

    ComPtr<ID3D12Resource> m_upload;
    uint8_t* m_uploadMapped = nullptr;

    ComPtr<ID3D12Resource> m_palettes[FRAMES_IN_FLIGHT];
    D3D12_RESOURCE_STATES m_paletteStates[FRAMES_IN_FLIGHT] = {};

    ComPtr<ID3D12Resource> m_paletteNormals[FRAMES_IN_FLIGHT];
    D3D12_RESOURCE_STATES m_paletteNormalStates[FRAMES_IN_FLIGHT] = {};

    ComPtr<ID3D12Resource> m_outputs[FRAMES_IN_FLIGHT];

    ComPtr<ID3D12Resource> m_dummyBuffer;

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
