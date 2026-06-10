// Deferred decal pixel shader.
// Runs after the G-Buffer pass and before the lighting pass.
// Reads the scene depth to reconstruct world position, checks if that position
// is inside the decal volume, and writes the decal albedo into the G-Buffer.

#include "Samplers.hlsli"

cbuffer CbDecal : register(b0)
{
    float4x4 MVP;        // unused in PS but kept for CB alignment
    float4x4 InvModel;   // world → decal local space
    float4x4 InvViewProj;// clip  → world space
    float4   ColourOpacity; // rgb = tint colour, a = opacity
};

Texture2D DepthMap    : register(t0);  // G-Buffer depth (R32_FLOAT)
Texture2D DecalAlbedo : register(t1);  // decal colour texture

// NDC xy → texture UV [0,1]^2 with Y-flip
float2 ndcToUV(float2 ndc)
{
    float2 uv = ndc * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    return uv;
}

// Reconstruct world-space position from UV + depth using the inverse VP matrix.
float3 reconstructWorldPos(float2 uv, float depth)
{
    float2 ndc    = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipH  = float4(ndc, depth, 1.0f);
    float4 worldH = mul(clipH, InvViewProj);
    return worldH.xyz / worldH.w;
}

// G-Buffer output – only albedo (RT0) is written.
// RT1 (normalMetalRough) write mask is set to 0 in the PSO.
struct PSOutput
{
    float4 albedo           : SV_TARGET0;
};

PSOutput main(float3 ndcPos : POSITION)
{
    float2 uv = ndcToUV(ndcPos.xy);

    // Reconstruct world position from scene depth
    float  depth    = DepthMap.Sample(PointClamp, uv).r;
    float3 worldPos = reconstructWorldPos(uv, depth);

    // Transform to decal local space
    float3 objPos = mul(float4(worldPos, 1.0f), InvModel).xyz;

    // Discard pixels outside the unit box volume
    if (abs(objPos.x) > 0.5f || abs(objPos.y) > 0.5f || abs(objPos.z) > 0.5f)
        discard;

    // Compute decal UV: project along local Z axis (front face = +Z side)
    float2 decalUV;
    decalUV.x =  objPos.x + 0.5f;
    decalUV.y = -objPos.y + 0.5f;  // flip Y so texture origin is top-left

    float4 colour = DecalAlbedo.Sample(BilinearWrap, decalUV);
    colour.rgb *= ColourOpacity.rgb;
    colour.a   *= ColourOpacity.a;

    // Discard transparent parts of the decal texture
    if (colour.a < 0.05f)
        discard;

    // Compute surface normal derivatives for normal-map decals (optional extension)
    // float3 tangent   = normalize(ddx(worldPos));
    // float3 bitangent = normalize(-ddy(worldPos));
    // float3 surfNorm  = cross(tangent, bitangent);

    PSOutput o;
    o.albedo = float4(colour.rgb, 1.0f);
    return o;
}
