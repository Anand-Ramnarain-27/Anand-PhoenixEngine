
struct GpuParticle {
    float3 position;
    float size;
    float4 color;
    float rotation;
    float2 uvMin;
    float2 uvMax;
};

StructuredBuffer<GpuParticle> Particles : register(t0);

cbuffer CbParticle : register(b0){
    float4x4 ViewProj;
    float4 CamRight;
    float4 CamUp;
};

struct VS_OUTPUT {
    float4 svPos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

static const float2 kCorners[4] =
{
    float2(-1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2(-1.0f, 1.0f),
    float2( 1.0f, 1.0f)
};

static const float2 kUVs[4] =
{
    float2(0.0f, 1.0f),
    float2(1.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f)
};

static const float kPI = 3.14159265358979f;

VS_OUTPUT main(uint vid : SV_VertexID, uint iid : SV_InstanceID){
    GpuParticle p = Particles[iid];

    float2 corner = kCorners[vid];

    float rad = p.rotation * (kPI / 180.0f);
    float s = sin(rad);
    float c = cos(rad);
    float2 rCorner = float2(corner.x * c - corner.y * s,
                            corner.x * s + corner.y * c);

    float halfSize = p.size * 0.5f;
    float3 worldPos = p.position
                    + CamRight.xyz * rCorner.x * halfSize
                    + CamUp.xyz * rCorner.y * halfSize;

    VS_OUTPUT o;
    o.svPos = mul(float4(worldPos, 1.0f), ViewProj);
    o.uv = lerp(p.uvMin, p.uvMax, kUVs[vid]);
    o.color = p.color;
    return o;
}
