// Decal vertex shader.
// Renders a unit box [-0.5, 0.5]^3 transformed by the decal MVP.
// The NDC position is passed to the pixel shader for depth-buffer sampling.

cbuffer CbDecal : register(b0)
{
    float4x4 MVP;
    float4x4 InvModel;
    float4x4 InvViewProj;
    float4   ColourOpacity; // rgb = tint colour, a = opacity (unused in VS)
};

struct VS_OUTPUT
{
    float3 ndcPos  : POSITION;
    float4 svPos   : SV_POSITION;
};

VS_OUTPUT main(float3 position : POSITION)
{
    VS_OUTPUT o;
    float4 clip  = mul(float4(position, 1.0f), MVP);
    o.svPos  = clip;
    o.ndcPos = clip.xyz / clip.w;  // perspective division → NDC
    return o;
}
