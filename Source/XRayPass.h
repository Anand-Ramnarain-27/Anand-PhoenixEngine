#pragma once

#include "ShaderTableDesc.h"
#include "RenderTargetDesc.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>

using Microsoft::WRL::ComPtr;

struct MeshEntry;
class GBufferPass;
class RenderTexture;

// Per-group silhouette style, already exposure-compensated by the caller.
struct XRayGroupStyle {
    Vector3 color = Vector3(0.35f, 0.85f, 1.0f);
    float fillAlpha = 0.45f;
    float outlineWidth = 1.5f;
};

// X-ray silhouette for tagged meshes hidden behind scene geometry.
//  1. Mask: redraws the tagged entries with the GBuffer's own VS and per-draw CBs (bit-identical depth) into an
//     R8G8 target, comparing against the GBuffer depth: R = group/255 where hidden, G = 1 where visible. MAX
//     blending resolves the character's self-overlap (a hidden back arm behind a visible chest stays visible).
//  2. Composite: fullscreen blend into the scene's HDR output where R > 0 && G == 0, plus an outline on the
//     silhouette's edge against non-character pixels.
class XRayPass {
public:
    static constexpr int kMaxGroups = 4;
    static constexpr int NUM_VIEWPORTS = 2;

    bool init(ID3D12Device* device);
    void cleanUp();

    // gbuffer must hold this frame's geometry for `viewportIndex` (depth in DEPTH_READ | PIXEL_SHADER_RESOURCE).
    // Leaves outputRT bound as the render target with the GBuffer's read-only DSV.
    void render(ID3D12GraphicsCommandList* cmd, GBufferPass& gbuffer,
                const std::vector<MeshEntry*>& xrayMeshes,
                const XRayGroupStyle* groups, int groupCount,
                RenderTexture* outputRT, uint32_t width, uint32_t height, int viewportIndex);

private:
    bool createMaskPipeline(ID3D12Device* device);
    bool createCompositePipeline(ID3D12Device* device);
    void ensureMask(int viewportIndex, uint32_t width, uint32_t height);
    void releaseMask(int viewportIndex);

    ComPtr<ID3D12RootSignature> m_maskRootSig;
    ComPtr<ID3D12PipelineState> m_maskPso;
    ComPtr<ID3D12PipelineState> m_maskPsoDoubleSided;

    ComPtr<ID3D12RootSignature> m_compositeRootSig;
    ComPtr<ID3D12PipelineState> m_compositePso;

    struct Mask {
        ComPtr<ID3D12Resource> texture;
        RenderTargetDesc rtv;
        ShaderTableDesc srv;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    Mask m_masks[NUM_VIEWPORTS];
};
