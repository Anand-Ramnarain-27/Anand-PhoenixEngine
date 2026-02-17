#pragma once
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class MeshPipeline
{
public:
    struct GPUDirectionalLight
    {
        Vector3 direction;
        float intensity;
        Vector3 color;
        float pad;
    };

    struct GPUPointLight
    {
        Vector3 position;
        float sqRadius; 
        Vector3 color;
        float intensity;
    };

    struct GPUSpotLight
    {
        Vector3 position;
        float sqRadius;
        Vector3 direction;
        float innerCos;  
        Vector3 color;
        float outerCos; 
        float intensity;
        Vector3 pad;  
    };

    struct LightCB
    {
        Vector3 ambientColor;
        float ambientIntensity;
        Vector3 viewPos;
        float pad0;
        uint32_t numDirLights;
        uint32_t numPointLights;
        uint32_t numSpotLights;
        uint32_t pad1;
        GPUDirectionalLight dirLights[2];
        GPUPointLight pointLight;
        GPUSpotLight spotLight;
    };

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

private:
    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;
};
