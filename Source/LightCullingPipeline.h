#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Root-signature + compute PSO for the tiled light-culling pass.
class LightCullingPipeline {
public:
    static constexpr UINT SLOT_CB = 0; // b0 – CbCulling
    static constexpr UINT SLOT_DEPTH = 1; // t0 – depth SRV
    static constexpr UINT SLOT_POINT_LIGHTS= 2; // t1 – PointLight SRV
    static constexpr UINT SLOT_SPOT_LIGHTS = 3; // t2 – SpotLight SRV
    static constexpr UINT SLOT_POINT_UAV = 4; // u0 – point light index list UAV
    static constexpr UINT SLOT_SPOT_UAV = 5; // u1 – spot  light index list UAV

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
