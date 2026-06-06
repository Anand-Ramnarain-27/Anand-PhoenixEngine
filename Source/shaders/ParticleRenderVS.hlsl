// ParticleRenderVS — reads particle data from a StructuredBuffer (SRV) and builds
// camera-facing billboard quads. Each instance = one particle; each vertex = quad corner.
// Dead particles (lifetime == 0) are collapsed to a degenerate triangle outside the view.

#include "Particle.hlsli"

cbuffer CbParticleRender : register(b0)
{
    float4x4 ViewProj;
    float3   CameraRight;   // view-space right vector (world space)
    float    HalfSize;      // half the billboard size in metres
    float3   CameraUp;      // view-space up vector (world space)
    float    cbPad;
};

StructuredBuffer<Particle> Particles : register(t0);

struct VSOut
{
    float4 pos     : SV_POSITION;
    float2 uv      : TEXCOORD0;
    float4 colour  : COLOR0;
};

// Two triangles forming a quad, wound CCW
static const float2 kOffsets[6] = {
    float2(-1, -1), float2(-1,  1), float2( 1,  1),
    float2(-1, -1), float2( 1,  1), float2( 1, -1)
};
static const float2 kUVs[6] = {
    float2(0, 1), float2(0, 0), float2(1, 0),
    float2(0, 1), float2(1, 0), float2(1, 1)
};

VSOut main(uint vertID : SV_VertexID, uint instID : SV_InstanceID)
{
    Particle p = Particles[instID];
    VSOut    o;

    // Dead particles → collapse to a degenerate triangle far off-screen (no rasterisation cost)
    if (p.lifetime <= 0.0f)
    {
        o.pos    = float4(1e9f, 1e9f, 1e9f, 1.0f);
        o.uv     = float2(0, 0);
        o.colour = float4(0, 0, 0, 0);
        return o;
    }

    float2 off     = kOffsets[vertID] * HalfSize;
    float3 worldPos = p.position
                    + CameraRight * off.x
                    + CameraUp    * off.y;

    o.pos    = mul(float4(worldPos, 1.0f), ViewProj);
    o.uv     = kUVs[vertID];
    o.colour = p.colour;
    return o;
}
