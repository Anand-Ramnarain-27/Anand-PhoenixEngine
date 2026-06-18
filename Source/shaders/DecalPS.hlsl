
#include "Samplers.hlsli"
#include "Decal.hlsli"

Texture2D DepthMap : register(t0);
Texture2D DecalAlbedo : register(t1);

float2 ndcToUV(float2 ndc){
    float2 uv = ndc * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    return uv;
}

float3 reconstructWorldPos(float2 uv, float depth){
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipH = float4(ndc, depth, 1.0f);
    float4 worldH = mul(clipH, InvViewProj);
    return worldH.xyz / worldH.w;
}

struct PSOutput {
    float4 albedo : SV_TARGET0;
};

PSOutput main(float3 ndcPos : POSITION){
    float2 uv = ndcToUV(ndcPos.xy);

    float depth = DepthMap.Sample(PointClamp, uv).r;
    float3 worldPos = reconstructWorldPos(uv, depth);

    float3 objPos = mul(float4(worldPos, 1.0f), InvModel).xyz;

    if (abs(objPos.x) > 0.5f || abs(objPos.y) > 0.5f || abs(objPos.z) > 0.5f)
        discard;

    float2 decalUV;
    decalUV.x = objPos.x + 0.5f;
    decalUV.y = -objPos.y + 0.5f;

    float4 colour = DecalAlbedo.Sample(BilinearWrap, decalUV);
    colour.rgb *= ColourOpacity.rgb;
    colour.a *= ColourOpacity.a;

    if (colour.a < 0.05f)
        discard;


    PSOutput o;
    o.albedo = float4(colour.rgb, 1.0f);
    return o;
}
