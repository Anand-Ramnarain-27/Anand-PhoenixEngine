#ifndef _PARTICLE_HLSLI_
#define _PARTICLE_HLSLI_

// Shared particle layout between the compute update shader and the render vertex shader.
// 48 bytes per particle — fits well in cache lines.
struct Particle
{
    float3 position;     // world-space position
    float  lifetime;     // seconds remaining (0 = inactive/dead)
    float3 velocity;     // metres per second
    float  maxLifetime;  // initial lifetime (used to lerp colour)
    float4 colour;       // RGBA tint, lerped from start→end over lifetime
};

#endif // _PARTICLE_HLSLI_
