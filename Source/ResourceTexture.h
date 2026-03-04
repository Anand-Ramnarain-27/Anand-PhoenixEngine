#pragma once
#include "ResourceBase.h"
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

class ResourceTexture : public ResourceBase
{
public:
    ResourceTexture(UID uid);
    ~ResourceTexture() override;

    bool LoadInMemory()     override;
    void UnloadFromMemory() override;

    ID3D12Resource* getResource() const { return m_texture.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSRV()      const { return m_srv; }
    bool                        hasTexture()  const { return m_texture != nullptr; }

private:
    ComPtr<ID3D12Resource>      m_texture;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srv = {};
};