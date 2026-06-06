#pragma once
#include "BloomPipeline.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;

// GPU-accelerated bloom post-process pass.
// Applies three compute shader dispatches (threshold → horizontal blur → vertical blur)
// followed by a fullscreen additive composite.
//
// Demonstrates lecture 14 concepts:
//   - UAV textures (RWTexture2D) created with ALLOW_UNORDERED_ACCESS
//   - Two-pass compute with UAV barriers between passes
//   - groupshared LDS cache in the blur shaders
//   - GroupMemoryBarrierWithGroupSync()
//   - Correct group count = (size + threads - 1) / threads
//   - Resource state transitions: PSR → UAV (compute write) → SRV (graphics read)
class BloomPass {
public:
    struct Settings {
        float threshold = 0.85f;   // luminance above which pixels glow
        float strength  = 1.2f;    // multiplier on extracted bright pixels
        bool  enabled   = true;
    };

    bool init(ID3D12Device* device, DXGI_FORMAT sceneRTFormat);

    // Run all bloom passes.
    // On entry: sceneRT is bound as RTV (rendering just finished).
    // On exit:  sceneRT is still bound as RTV with bloom additively composited.
    void render(ID3D12GraphicsCommandList* cmd,
                RenderTexture& sceneRT,
                uint32_t width, uint32_t height);

    Settings& getSettings() { return m_settings; }

private:
    struct CbBloom {
        uint32_t texW, texH;
        float    threshold;
        float    strength;
    };

    bool createIntermediateTextures(ID3D12Device* device, uint32_t w, uint32_t h);
    void releaseIntermediateTextures();
    void uploadCB(uint32_t w, uint32_t h);

    BloomPipeline m_pipeline;
    Settings      m_settings;

    // Two ping-pong UAV textures for the blur passes
    ComPtr<ID3D12Resource> m_texA;   // threshold output / blur final output
    ComPtr<ID3D12Resource> m_texB;   // horizontal blur intermediate
    uint32_t m_allocW = 0;
    uint32_t m_allocH = 0;

    // Descriptors for texA (SRV + UAV) and texB (SRV + UAV)
    ShaderTableDesc m_texA_SRV, m_texA_UAV;
    ShaderTableDesc m_texB_SRV, m_texB_UAV;

    // Constant buffer (upload heap, persistently mapped)
    ComPtr<ID3D12Resource> m_cb;
    void* m_cbMapped = nullptr;
};
