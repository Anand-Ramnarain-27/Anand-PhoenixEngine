#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

struct MeshEntry;
class ComponentAnimation;

// Slide 26-29: one output buffer + one palette buffer per frame in flight.
// Both sized to hold ALL skinned instances in one frame.
class SkinningPass {
public:
    static constexpr uint32_t MAX_SKINNED_VERTICES = 500000;
    static constexpr uint32_t MAX_PALETTE_MATRICES = 1024;   // total joints all instances
    static constexpr uint32_t MAX_MORPH_WEIGHTS = 2048;   // Phase 3

    bool init(ID3D12Device* device);

    // Upload matrix palettes + morph weights then dispatch compute for
    // every skinned mesh. Returns byte offsets into the output buffer.
    // Call before render pass each frame.
    void execute(
        ID3D12GraphicsCommandList* cmd,
        uint32_t frameIndex,
        const std::vector<ComponentAnimation*>& animComponents);

    // Get GPU virtual address of the skinned vertex data for a given
    // instance slot (returned by execute).
    D3D12_GPU_VIRTUAL_ADDRESS getOutputVA(uint32_t frameIndex, uint32_t byteOffset) const;

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    // Slide 29: upload = staging buffer (CPU write), palette/output = GPU
    ComPtr<ID3D12Resource> m_upload;          // single staging buffer
    uint8_t* m_uploadPtr = nullptr;

    static constexpr uint32_t FRAMES = 3;
    ComPtr<ID3D12Resource> m_palettes[FRAMES];  // B^-1 * T matrices + morph weights
    ComPtr<ID3D12Resource> m_outputs[FRAMES];   // skinned vertex results

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    uint32_t m_uploadOffsetBytes = 0;  // current write position in upload buffer
};
