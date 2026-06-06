// BloomCompositePS — additively blends the blurred bloom texture over the scene.
// Called via a fullscreen triangle (FullScreenVS.cso) with additive blending enabled.
// The bloom texture is the final vertical-blur output, transitioned from UAV → SRV
// before this graphics pass (resource state transition from compute → pixel shader).

#include "Samplers.hlsli"

Texture2D BloomTex : register(t0);  // SRV (bloom UAV transitioned to SRV before this pass)

float4 main(float2 uv : TEXCOORD, float4 pos : SV_Position) : SV_TARGET
{
    return float4(BloomTex.Sample(BilinearClamp, uv).rgb, 0.0f);
    // alpha = 0 so additive blend adds rgb without affecting scene alpha channel
}
