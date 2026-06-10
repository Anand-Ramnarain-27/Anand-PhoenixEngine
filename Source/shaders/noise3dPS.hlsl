// noise3dPS.hlsl — 3D gradient noise + 8-octave fractional Brownian motion
// pixel shader.  Direct GPU port of the reference "noise3d.hlsl" (Carlos Fuentes)
// and of the CPU Noise.h library used by ComponentParticleSystem turbulence.
//
// Apply to any geometry whose VS outputs WorldPos : TEXCOORD0.  The result is
// an animated greyscale pattern driven by world-space position and elapsed time.
//
// This shader is VISUAL ONLY — it writes to SV_TARGET and has no UAV outputs.
// The noise algorithm is also implemented on the CPU (Noise.h: gradientNoise3D,
// fbm3D) and is used there to drive particle turbulence velocity perturbation.
//
// Cross-system note (Lecture 12):
//   CPU path  — Noise::fbm3D() → ComponentParticleSystem turbulence → particle velocity
//   GPU path  — this PS shows the same field visually on geometry surfaces
//   GPU CS    — ParticleUpdateCS.hlsl applies the same algorithm in compute to GPU particles

cbuffer cbNoisePerFrame : register(b0)
{
    float4x4 matVP;    // view-projection  (used by noise3dVS, not this PS)
    float4x4 matGeo;   // model matrix     (used by noise3dVS, not this PS)
    float    uTime;    // elapsed time in seconds — animates the noise field
    float3   _pad;
};

struct PSInput
{
    float4 svPos    : SV_POSITION;
    float3 WorldPos : TEXCOORD0;   // world-space fragment position
};

// ============================================================
//  Hash / pseudo-random utilities
//  Matches the reference algorithm and CPU Noise.h exactly.
// ============================================================

// lowbias32 — XOR-shift-multiply avalanche hash (near-uniform 32-bit distribution).
uint hashU(uint x)
{
    x = (x ^ (x >> 16)) * 0x21f0aaadU;
    x = (x ^ (x >> 15)) * 0x735a2d97U;
    return x ^ (x >> 15);
}
uint hash2U(uint2 v) { return hashU(v.x ^ hashU(v.y)); }
uint hash3U(uint3 v) { return hashU(v.x ^ hash2U(v.yz)); }

// Maps the top 23 bits of a hash into the mantissa of an IEEE 754 float,
// giving a uniformly distributed value in [0, 1).
// asfloat(0x33800000) == 2^{-23}  (shifts mantissa bits into the [0,1) range).
float hash2Float(uint h)
{
    return (float)(h >> 8) * asfloat(0x33800000);
}

// ============================================================
//  Gradient generation
//  Maps an integer lattice cell to a random unit vector on the
//  unit sphere using spherical coordinates (uniform distribution).
//  Two consecutive hash values give the polar angle and azimuth.
// ============================================================
float3 grad(uint3 cell)
{
    uint h0  = hash3U(cell);
    uint h1  = hashU(h0);
    float ct = 2.0f * hash2Float(h0) - 1.0f;           // cos(theta) in [-1, 1]
    float st = sqrt(max(0.0f, 1.0f - ct * ct));         // sin(theta) >= 0
    float ph = 6.28318530718f * hash2Float(h1);         // phi in [0, 2*pi)
    return float3(cos(ph) * st, sin(ph) * st, ct);
}

// ============================================================
//  3D Gradient (Perlin) noise — quintic-eased trilinear blend
//  over the 8 corners of the lattice cell containing p.
//  Output range: approximately [-1, 1].
// ============================================================
float gradientNoise(float3 p)
{
    int3   cell = (int3)floor(p);
    float3 f    = frac(p);
    // Quintic easing: 6t^5 - 15t^4 + 10t^3  (C2-continuous at cell boundaries)
    float3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float3 g0 = grad(uint3(cell + int3(0,0,0)));
    float3 g1 = grad(uint3(cell + int3(1,0,0)));
    float3 g2 = grad(uint3(cell + int3(0,1,0)));
    float3 g3 = grad(uint3(cell + int3(1,1,0)));
    float3 g4 = grad(uint3(cell + int3(0,0,1)));
    float3 g5 = grad(uint3(cell + int3(1,0,1)));
    float3 g6 = grad(uint3(cell + int3(0,1,1)));
    float3 g7 = grad(uint3(cell + int3(1,1,1)));

    // Dot products: gradient · (p - corner)
    float v0 = dot(g0, f - float3(0,0,0));
    float v1 = dot(g1, f - float3(1,0,0));
    float v2 = dot(g2, f - float3(0,1,0));
    float v3 = dot(g3, f - float3(1,1,0));
    float v4 = dot(g4, f - float3(0,0,1));
    float v5 = dot(g5, f - float3(1,0,1));
    float v6 = dot(g6, f - float3(0,1,1));
    float v7 = dot(g7, f - float3(1,1,1));

    float front = lerp(lerp(v0, v1, u.x), lerp(v2, v3, u.x), u.y);
    float back  = lerp(lerp(v4, v5, u.x), lerp(v6, v7, u.x), u.y);
    return lerp(front, back, u.z);
}

// ============================================================
//  8-octave Fractional Brownian Motion
//  Matches the reference exactly:
//    frequency: 0.1, 0.2, 0.4, ... (doubles each octave)
//    amplitude: 0.5, 0.25, 0.125, ... (halves each octave)
//    per-octave bias: +0.5 shifts [-1,1] to [-0.5, 1.5]
//  Effective output range: approximately [0, 1].
// ============================================================
float fbm(float3 st)
{
    float value     = 0.0f;
    float amplitude = 0.5f;
    float frequency = 0.1f;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        value     += amplitude * (gradientNoise(st * frequency) + 0.5f);
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

// ============================================================
//  Entry point
//  World position is scaled × 24 to set the base spatial
//  frequency, then uTime is added to scroll/animate the field.
//  Output is greyscale (R=G=B).
// ============================================================
float4 main(PSInput pin) : SV_TARGET
{
    float res = fbm(pin.WorldPos * 24.0f + uTime);
    return float4(res, res, res, 1.0f);
}
