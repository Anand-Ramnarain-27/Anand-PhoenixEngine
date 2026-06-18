#ifndef _NOISE_HLSLI_
#define _NOISE_HLSLI_

uint hashU(uint x){
    x = (x ^ (x >> 16)) * 0x21f0aaadU;
    x = (x ^ (x >> 15)) * 0x735a2d97U;
    return x ^ (x >> 15);
}
uint hash2U(uint2 v){ return hashU(v.x ^ hashU(v.y)); }
uint hash3U(uint3 v){ return hashU(v.x ^ hash2U(v.yz)); }

float hash2Float(uint h){ return (float)(h >> 8) * asfloat(0x33800000); }

#endif
