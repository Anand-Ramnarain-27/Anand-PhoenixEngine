// ParticleRenderVS.hlsl — Instanced GPU particle vertex shader.
//
// Each particle draw uses DrawInstanced(4, liveCount, 0, 0) with TRIANGLESTRIP.
//   SV_InstanceID — indexes into the GpuParticle StructuredBuffer.
//   SV_VertexID   — selects the billboard corner (0-3).
//
// The GpuParticle struct has pre-baked world position, size, color, rotation, and
// sheet-atlas UV rect.  All over-lifetime curves (color, size) are evaluated on the
// CPU in ParticlePass::bakeParticles() so the VS stays minimal and the GPU particles
// can coexist with the CPU simulation without code duplication.
//
// Camera right/up axes come from the per-emitter constant buffer and are shared
// across all vertices in the instance, giving screen-aligned billboards identical
// in appearance to the existing BillboardPass quads.

struct GpuParticle
{
    float3 position;  // world-space centre
    float  size;      // world-space diameter (width = height)
    float4 color;     // premultiplied RGBA
    float  rotation;  // billboard rotation in degrees (around view axis)
    float2 uvMin;     // sheet atlas tile — bottom-left UV
    float2 uvMax;     // sheet atlas tile — top-right UV
};

StructuredBuffer<GpuParticle> Particles : register(t0);

cbuffer CbParticle : register(b0)
{
    float4x4 ViewProj;   // combined view-projection
    float4   CamRight;   // xyz = camera right unit vector, w unused
    float4   CamUp;      // xyz = camera up    unit vector, w unused
};

struct VS_OUTPUT
{
    float4 svPos : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

// Corner table: billboard local-space positions.
// Wound for TRIANGLESTRIP: 0-1-2 (front), 1-3-2 (front).
static const float2 kCorners[4] =
{
    float2(-1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2(-1.0f,  1.0f),
    float2( 1.0f,  1.0f)
};

// UV table aligned with kCorners (V is flipped: 0 = top of texture, 1 = bottom).
static const float2 kUVs[4] =
{
    float2(0.0f, 1.0f),
    float2(1.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f)
};

static const float kPI = 3.14159265358979f;

VS_OUTPUT main(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    GpuParticle p = Particles[iid];

    float2 corner = kCorners[vid];

    // Apply per-particle rotation around the view axis.
    float rad = p.rotation * (kPI / 180.0f);
    float s   = sin(rad);
    float c   = cos(rad);
    float2 rCorner = float2(corner.x * c - corner.y * s,
                            corner.x * s + corner.y * c);

    // Expand to a camera-facing quad in world space.
    float halfSize = p.size * 0.5f;
    float3 worldPos = p.position
                    + CamRight.xyz * rCorner.x * halfSize
                    + CamUp.xyz    * rCorner.y * halfSize;

    VS_OUTPUT o;
    o.svPos = mul(float4(worldPos, 1.0f), ViewProj);
    // Map normalised corner UV into the per-particle sheet tile rect.
    o.uv    = lerp(p.uvMin, p.uvMax, kUVs[vid]);
    o.color = p.color;
    return o;
}
