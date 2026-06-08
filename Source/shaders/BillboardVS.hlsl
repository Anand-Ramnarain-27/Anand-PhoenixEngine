// Billboard vertex shader.
// Builds a camera-aligned quad from a single per-instance constant buffer —
// no vertex/index buffers are bound; the 4 corners come from SV_VertexID.
// The CPU side (BillboardPass) precomputes the quad's right/up axes according
// to the component's alignment mode (screen / world / axial).

cbuffer CbBillboard : register(b0)
{
    float4x4 ViewProj;
    float4 CenterHalfWidth;   // xyz = world-space centre, w = half width
    float4 RightHalfHeight;   // xyz = right axis (unit),  w = half height
    float4 Up;                // xyz = up axis (unit)
    float4 Tint;              // rgba tint multiplier
    float4 FrameRectA;        // u0,v0,u1,v1 — current sheet tile
    float4 FrameRectB;        // u0,v0,u1,v1 — next sheet tile (for interpolation)
    float4 BlendFactor;       // x = blend between tile A and tile B
};

struct VS_OUTPUT
{
    float4 svPos : SV_POSITION;
    float2 uvA   : TEXCOORD0;
    float2 uvB   : TEXCOORD1;
    float  blend : TEXCOORD2;
    float4 tint  : COLOR0;
};

// Quad corners in [-1,1] local space, wound for a triangle strip (CCW front face)
static const float2 kCorners[4] = {
    float2(-1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2(-1.0f,  1.0f),
    float2( 1.0f,  1.0f)
};

VS_OUTPUT main(uint vid : SV_VertexID)
{
    VS_OUTPUT o;
    float2 corner = kCorners[vid];

    float3 center = CenterHalfWidth.xyz;
    float3 right  = RightHalfHeight.xyz;
    float3 up     = Up.xyz;

    float3 worldPos = center
                    + right * (corner.x * CenterHalfWidth.w)
                    + up    * (corner.y * RightHalfHeight.w);

    o.svPos = mul(float4(worldPos, 1.0f), ViewProj);

    // Local UV: (0,0) top-left .. (1,1) bottom-right (V flipped vs. corner.y)
    float2 localUV = float2(corner.x * 0.5f + 0.5f, 0.5f - corner.y * 0.5f);

    o.uvA   = lerp(FrameRectA.xy, FrameRectA.zw, localUV);
    o.uvB   = lerp(FrameRectB.xy, FrameRectB.zw, localUV);
    o.blend = BlendFactor.x;
    o.tint  = Tint;
    return o;
}
