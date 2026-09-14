#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;
class GBufferPass;

struct FogSettings {
    enum class Mode : uint32_t { Linear = 0, ExponentialHeight = 1 };

    bool enabled = false;
    Mode mode = Mode::Linear;
    Vector3 color = Vector3(0.5f, 0.55f, 0.6f);
    float startDistance = 10.0f;
    float endDistance = 100.0f;
    float maxOpacity = 1.0f;
    float density = 0.02f;
    float heightFalloff = 0.1f;
    float heightOffset = 0.0f;
};

class FogPass {
public:
    static constexpr int NUM_VIEWPORTS = 2;

    bool init(ID3D12Device* device);

    // Returns the RenderTexture the fogged result was written to (either output, or input if fog is disabled/invalid).
    RenderTexture* render(ID3D12GraphicsCommandList* cmd,
                          RenderTexture* input,
                          RenderTexture* output,
                          GBufferPass& gbufferPass,
                          const Vector3& cameraPos,
                          const Matrix& invViewProj,
                          const FogSettings& settings,
                          int viewportIndex);

private:
    struct CbPerFrame {
        Matrix invViewProj;
        Vector3 cameraPosition;
        float framePad0;
        Vector3 fogColor;
        float framePad1;
        float startDistance;
        float endDistance;
        float maxOpacity;
        uint32_t mode;
        float density;
        float heightFalloff;
        float heightOffset;
        float framePad2;
    };

    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);
    bool createUploadBuffers(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_perFrameCB[NUM_VIEWPORTS];
    void* m_perFrameMapped[NUM_VIEWPORTS] = {};
};
