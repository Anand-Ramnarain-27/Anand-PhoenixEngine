#pragma once
#include "RenderTexture.h"
#include <memory>

class GBuffer {
public:
    void resize(uint32_t w, uint32_t h);
    bool isValid() const;

    RenderTexture* getAlbedo()   const { return m_albedo.get(); }
    RenderTexture* getNormalMR() const { return m_normalMR.get(); }
    RenderTexture* getEmissive() const { return m_emissive.get(); }

    // CPU RTV handles for OMSetRenderTargets (call before geometry pass)
    void getRTVHandles(D3D12_CPU_DESCRIPTOR_HANDLE out[3]) const;

    // GPU SRV handles for binding to lighting pass root tables
    D3D12_GPU_DESCRIPTOR_HANDLE getAlbedoSRV()   const;
    D3D12_GPU_DESCRIPTOR_HANDLE getNormalMRSRV() const;
    D3D12_GPU_DESCRIPTOR_HANDLE getEmissiveSRV() const;

    // Transition all 3 RTs to the requested state
    void transitionAll(ID3D12GraphicsCommandList* cmd,
        D3D12_RESOURCE_STATES from,
        D3D12_RESOURCE_STATES to);

private:
    std::unique_ptr<RenderTexture> m_albedo;
    std::unique_ptr<RenderTexture> m_normalMR;
    std::unique_ptr<RenderTexture> m_emissive;
};
