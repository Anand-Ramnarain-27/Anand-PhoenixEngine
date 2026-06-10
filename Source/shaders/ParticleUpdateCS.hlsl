// ParticleUpdateCS.hlsl — GPU turbulence compute shader for particles.
//
// Cross-system connection: Lecture 12 "Noise" → Particle velocity (GPU path).
// Mirrors ComponentParticleSystem::update()'s turbulence logic exactly, but runs
// on the GPU so the CPU can skip its own turbulence pass when useGPU is true.
//
// The CPU uploads a StructuredBuffer<GpuParticle> to the upload heap (t0, read).
// This CS writes perturbed positions into a UAV buffer (u0, write).
// A UAV barrier separates the dispatch from the subsequent render draw.
//
// Thread groups: [64, 1, 1].  Dispatch: ceil(liveCount / 64) groups.
//
// Noise algorithm: identical to CPU Noise.h (gradientNoise3D, fbm3D) and to
// noise3dPS.hlsl — same hash constants, same spherical gradients, same FBM loop.

// ---- GpuParticle layout (must match ParticlePass.h) -------------------------
struct GpuParticle
{
    float3 position;
    float  size;
    float4 color;
    float  rotation;
    float2 uvMin;
    float2 uvMax;
};

// Input: CPU-uploaded particle positions (read-only, upload heap).
StructuredBuffer<GpuParticle>   InputParticles  : register(t0);
// Output: CS-modified positions written for the render VS.
RWStructuredBuffer<GpuParticle> OutputParticles : register(u0);

cbuffer CbUpdate : register(b0)
{
    uint  ActiveCount;     // number of live particles to process
    float DeltaTime;       // frame delta time (seconds)
    float TurbFrequency;   // noise sampling frequency (world-space scale)
    float TurbStrength;    // max added speed (m/s)
    float TurbScrollSpeed; // noise field scroll speed along +Y (m/s)
    float Time;            // total elapsed time (for scroll offset)
    float2 _pad;
};

// ============================================================
//  Noise implementation — identical to noise3dPS.hlsl and CPU Noise.h
// ============================================================
uint hashU(uint x)
{
    x = (x ^ (x >> 16)) * 0x21f0aaadU;
    x = (x ^ (x >> 15)) * 0x735a2d97U;
    return x ^ (x >> 15);
}
uint hash2U(uint2 v) { return hashU(v.x ^ hashU(v.y)); }
uint hash3U(uint3 v) { return hashU(v.x ^ hash2U(v.yz)); }

float hash2Float(uint h) { return (float)(h >> 8) * asfloat(0x33800000); }

float3 grad3(int3 cell)
{
    uint h0  = hash3U(uint3(cell));
    uint h1  = hashU(h0);
    float ct = 2.0f * hash2Float(h0) - 1.0f;
    float st = sqrt(max(0.0f, 1.0f - ct * ct));
    float ph = 6.28318530718f * hash2Float(h1);
    return float3(cos(ph)*st, sin(ph)*st, ct);
}

float gradientNoise3D(float3 p)
{
    int3   cell = (int3)floor(p);
    float3 f    = frac(p);
    float3 u    = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float front = lerp(lerp(dot(grad3(cell+int3(0,0,0)), f-float3(0,0,0)),
                            dot(grad3(cell+int3(1,0,0)), f-float3(1,0,0)), u.x),
                       lerp(dot(grad3(cell+int3(0,1,0)), f-float3(0,1,0)),
                            dot(grad3(cell+int3(1,1,0)), f-float3(1,1,0)), u.x), u.y);
    float back  = lerp(lerp(dot(grad3(cell+int3(0,0,1)), f-float3(0,0,1)),
                            dot(grad3(cell+int3(1,0,1)), f-float3(1,0,1)), u.x),
                       lerp(dot(grad3(cell+int3(0,1,1)), f-float3(0,1,1)),
                            dot(grad3(cell+int3(1,1,1)), f-float3(1,1,1)), u.x), u.y);
    return lerp(front, back, u.z);
}

// 3-octave fBm (enough for turbulence; full 8-octave is too heavy in CS)
float fbm3(float3 p)
{
    float v = 0.0f, a = 0.5f, freq = 1.0f;
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        v    += a * gradientNoise3D(p * freq);
        freq *= 2.0f;
        a    *= 0.5f;
    }
    return v;
}

// ============================================================
//  Main dispatch
// ============================================================
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= ActiveCount) return;

    GpuParticle p = InputParticles[dtid.x];

    // ---- Noise flow field (Lecture 12 "Noise → Particle velocity" exercise) ----
    // Sample the 3D gradient noise field at the particle's scrolling world position.
    //   angle = clamp(noise * 0.5 + 0.5, 0, 1) * 2*pi
    //   flow  = (cos(angle), 0, sin(angle))            — XZ plane flow vector
    // A second offset sample modulates the strength (0..turbStrength m/s).
    float3 scrollOffset = float3(0.0f, Time * TurbScrollSpeed, 0.0f);
    float3 samplePos    = p.position * TurbFrequency + scrollOffset;

    float angleNoise    = fbm3(samplePos);
    float strengthNoise = fbm3(samplePos + float3(37.13f, -91.7f, 5.21f));

    float angle    = clamp(angleNoise * 0.5f + 0.5f, 0.0f, 1.0f) * 6.28318530718f;
    float3 flow    = float3(cos(angle), 0.0f, sin(angle));
    float strength = clamp(strengthNoise * 0.5f + 0.5f, 0.0f, 1.0f) * TurbStrength;

    // Perturb position (equivalent to "velocity += flow * strength; position += velocity * dt"
    // but since we only store position in GpuParticle we integrate directly).
    p.position += flow * strength * DeltaTime;

    OutputParticles[dtid.x] = p;
}
