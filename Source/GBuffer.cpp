#include "Globals.h"
#include "GBuffer.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include <d3dx12.h>

void GBuffer::resize(uint32_t w, uint32_t h) {
    if (!m_albedo) {
        m_albedo = std::make_unique<RenderTexture>(
            "GBuf_Albedo", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0, 0, 0, 0));
        m_normalMR = std::make_unique<RenderTexture>(
            "GBuf_NormalMR", DXGI_FORMAT_R16G16B16A16_FLOAT, Vector4(0, 0, 0, 0));
        m_emissive = std::make_unique<RenderTexture>(
            "GBuf_Emissive", DXGI_FORMAT_R16G16B16A16_FLOAT, Vector4(0, 0, 0, 0));
    }
    m_albedo->resize(w, h);
    m_normalMR->resize(w, h);
    m_emissive->resize(w, h);
}

bool GBuffer::isValid() const {
    return m_albedo && m_albedo->isValid() &&
        m_normalMR && m_normalMR->isValid() &&
        m_emissive && m_emissive->isValid();
}

void GBuffer::getRTVHandles(D3D12_CPU_DESCRIPTOR_HANDLE out[3]) const {
    out[0] = m_albedo->getRtvHandle();
    out[1] = m_normalMR->getRtvHandle();
    out[2] = m_emissive->getRtvHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::getAlbedoSRV()   const { return m_albedo->getSrvHandle(); }
D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::getNormalMRSRV() const { return m_normalMR->getSrvHandle(); }
D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::getEmissiveSRV() const { return m_emissive->getSrvHandle(); }

void GBuffer::transitionAll(ID3D12GraphicsCommandList* cmd,
    D3D12_RESOURCE_STATES from,
    D3D12_RESOURCE_STATES to) {
    CD3DX12_RESOURCE_BARRIER barriers[3] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_albedo->getTexture(),   from, to),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_normalMR->getTexture(), from, to),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_emissive->getTexture(), from, to),
    };
    cmd->ResourceBarrier(3, barriers);
}
