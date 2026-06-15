
cbuffer CbBillboard : register(b0){
    float4x4 ViewProj;
    float4 CenterHalfWidth;
    float4 RightHalfHeight;
    float4 Up;
    float4 Tint;
    float4 FrameRectA;
    float4 FrameRectB;
    float4 BlendFactor;
};

struct VS_OUTPUT {
    float4 svPos : SV_POSITION;
    float2 uvA : TEXCOORD0;
    float2 uvB : TEXCOORD1;
    float blend : TEXCOORD2;
    float4 tint : COLOR0;
};

static const float2 kCorners[4] = {
    float2(-1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2(-1.0f, 1.0f),
    float2( 1.0f, 1.0f)
};

VS_OUTPUT main(uint vid : SV_VertexID){
    VS_OUTPUT o;
    float2 corner = kCorners[vid];

    float3 center = CenterHalfWidth.xyz;
    float3 right = RightHalfHeight.xyz;
    float3 up = Up.xyz;

    float3 worldPos = center
                    + right * (corner.x * CenterHalfWidth.w)
                    + up * (corner.y * RightHalfHeight.w);

    o.svPos = mul(float4(worldPos, 1.0f), ViewProj);

    float2 localUV = float2(corner.x * 0.5f + 0.5f, 0.5f - corner.y * 0.5f);

    o.uvA = lerp(FrameRectA.xy, FrameRectA.zw, localUV);
    o.uvB = lerp(FrameRectB.xy, FrameRectB.zw, localUV);
    o.blend = BlendFactor.x;
    o.tint = Tint;
    return o;
}
