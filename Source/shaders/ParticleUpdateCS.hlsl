// ParticleUpdateCS — Lecture 14 (GPU Architecture & Compute):
//   - GPGPU physics simulation (one of the primary use cases from the lecture)
//   - RWStructuredBuffer as a UAV for concurrent read/write particle state
//   - InterlockedAdd (atomic) to safely count dead particles without race conditions
//   - SV_DispatchThreadID maps one thread to one particle
//   - [numthreads(256, 1, 1)] — multiple of the Warp/Wave size (32/64)
//   - Group count = (MaxParticles + 255) / 256  (lecture formula: (N + threads - 1) / threads)
//   - Bounds check prevents threads in the last partial group from touching stale data

#include "Particle.hlsli"

cbuffer CbParticle : register(b0)
{
    float3 Gravity;        // e.g. (0, -9.81, 0)
    float  DeltaTime;      // frame time in seconds
    float4 ColourStart;    // particle colour at birth
    float4 ColourEnd;      // particle colour at death
    uint   MaxParticles;   // total buffer capacity
    float3 cbPad;
};

RWStructuredBuffer<Particle> Particles : register(u0);   // particle state UAV
RWBuffer<uint>               DeadCount : register(u1);   // atomic dead-particle counter

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // BOUNDS CHECK — last partial group may have threads beyond MaxParticles.
    if (id.x >= MaxParticles)
        return;

    Particle p = Particles[id.x];

    // Skip inactive particles (lifetime == 0 means the slot is empty)
    if (p.lifetime <= 0.0f)
        return;

    // ── Physics integration ──────────────────────────────────────────────
    p.velocity += Gravity * DeltaTime;
    p.position += p.velocity * DeltaTime;
    p.lifetime -= DeltaTime;

    // ── Colour interpolation ──────────────────────────────────────────────
    float t   = saturate(1.0f - p.lifetime / max(p.maxLifetime, 0.0001f));
    p.colour  = lerp(ColourStart, ColourEnd, t);

    // ── Death handling + atomic counter ──────────────────────────────────
    // InterlockedAdd: guaranteed uninterrupted — no race condition across threads.
    // Note: atomic operations are globally visible; no globallycoherent needed.
    if (p.lifetime <= 0.0f)
    {
        p.lifetime = 0.0f;
        uint oldVal;
        InterlockedAdd(DeadCount[0], 1u, oldVal);   // safely count dead particles
    }

    // Write updated particle back to UAV — unordered but each thread writes its own slot.
    Particles[id.x] = p;
}
