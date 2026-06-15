
cbuffer CbTrail : register(b0){
    float4x4 ViewProj;
    float4 Tint;
};

struct VS_INPUT {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VS_OUTPUT {
    float4 svPos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VS_OUTPUT main(VS_INPUT i){
    VS_OUTPUT o;
    o.svPos = mul(float4(i.position, 1.0f), ViewProj);
    o.uv = i.uv;
    o.color = i.color * Tint;
    return o;
}
