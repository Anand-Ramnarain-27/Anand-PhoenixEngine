#ifndef _SKINVERTEX_HLSLI_
#define _SKINVERTEX_HLSLI_

struct Vertex {
    float3 position;
    float2 texCoord;
    float3 normal;
    float4 tangent;
};

struct BoneWeight {
    int4 indices;
    float4 weights;
};

#endif
