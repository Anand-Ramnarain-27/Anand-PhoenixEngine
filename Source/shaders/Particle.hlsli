#ifndef _PARTICLE_HLSLI_
#define _PARTICLE_HLSLI_

struct GpuParticle {
    float3 position;
    float size;
    float4 color;
    float rotation;
    float2 uvMin;
    float2 uvMax;
};

#endif
