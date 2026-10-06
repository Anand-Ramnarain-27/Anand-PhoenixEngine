
#include "Samplers.hlsli"
#include "Decal.hlsli"

Texture2D DepthMap : register(t0);
Texture2D DecalAlbedo : register(t1);

float3 reconstructWorldPos(float2 uv, float depth){
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipH = float4(ndc, depth, 1.0f);
    float4 worldH = mul(clipH, InvViewProj);
    return worldH.xyz / worldH.w;
}

struct PSOutput {
    float4 albedo : SV_TARGET0;
    float4 emissive : SV_TARGET2;
};

PSOutput main(float4 svPos : SV_POSITION){
    uint w, h;
    DepthMap.GetDimensions(w, h);
    float2 uv = svPos.xy / float2(w, h);

    float depth = DepthMap.Sample(PointClamp, uv).r;
    if (depth >= 1.0f)
        discard;   // nothing drawn here (sky): a decal needs a surface
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

    if (colour.a < 0.02f)
        discard;

    // Albedo blends by alpha x mix (mix 0 = a pure glow decal that leaves the surface colour alone); emissive adds
    // colour x strength, so rings and glyphs read in unlit rooms.
    PSOutput o;
    o.albedo = float4(colour.rgb, colour.a * saturate(EmissiveAlbedoMix.w));
    o.emissive = float4(colour.rgb * EmissiveAlbedoMix.rgb, colour.a);
    return o;
}
