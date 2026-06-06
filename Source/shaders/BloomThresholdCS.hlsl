// BloomThresholdCS — Lecture 14 (DirectX Compute): UAVs, bounds checking, group count formula.
// Extracts bright pixels above a luminance threshold for the bloom effect.
// [numthreads(8, 8, 1)] → group count = (w+7)/8 × (h+7)/8 (lecture formula: (size+n-1)/n)

cbuffer CbBloom : register(b0)
{
    uint2 TextureSize;   // actual viewport size (prevents out-of-bounds writes)
    float Threshold;     // luminance cutoff (e.g. 0.8)
    float Strength;      // bloom multiplier
};

Texture2D<float4>   SceneIn  : register(t0);  // scene colour SRV (read-only)
RWTexture2D<float4> BloomOut : register(u0);  // bloom output UAV (read/write)

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // BOUNDS CHECK — required when (size + threads - 1) / threads rounds the group count up.
    // Without this, threads in the last partial group would write outside the texture.
    if (id.x >= TextureSize.x || id.y >= TextureSize.y)
        return;

    float4 colour = SceneIn[id.xy];

    // Perceived luminance (BT.709 weights)
    float lum    = dot(colour.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float excess = max(lum - Threshold, 0.0f) / max(lum, 0.0001f);

    BloomOut[id.xy] = float4(colour.rgb * excess * Strength, 1.0f);
}
