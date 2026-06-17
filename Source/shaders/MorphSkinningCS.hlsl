
#include "SkinVertex.hlsli"

struct MorphVertex {
    float3 deltaPosition; float _pad0;
    float3 deltaNormal; float _pad1;
    float3 deltaTangent; float _pad2;
};

cbuffer SkinCB : register(b0){
    uint g_numVertices;
    uint g_paletteOffset;
    uint g_vertexOffset;
    uint g_numMorphTargets;
    uint g_numJoints;
};

StructuredBuffer<float4x4> palette : register(t0);
StructuredBuffer<float4x4> paletteNormal : register(t1);
StructuredBuffer<Vertex> inVertex : register(t2);
StructuredBuffer<BoneWeight> boneWeights : register(t3);
StructuredBuffer<MorphVertex> morphVertices : register(t4);
StructuredBuffer<float> morphWeights : register(t5);
RWStructuredBuffer<Vertex> outVertex : register(u0);

Vertex morphVertex(uint index){
    Vertex v = inVertex[index];
    if (g_numMorphTargets == 0)
        return v;

    float3 deltaPos = (float3)0;
    float3 deltaNrm = (float3)0;
    float3 deltaTan = (float3)0;

    for (uint i = 0; i < g_numMorphTargets; ++i){
        float w = morphWeights[i];
        MorphVertex mv = morphVertices[i * g_numVertices + index];
        deltaPos += w * mv.deltaPosition;
        deltaNrm += w * mv.deltaNormal;
        deltaTan += w * mv.deltaTangent;
    }

    v.position += deltaPos;
    if (dot(deltaPos, deltaPos) > 1.0e6f
        || any(isinf(v.position)) || any(isnan(v.position)))
        v.position = inVertex[index].position;
    float3 blendedNrm = v.normal + deltaNrm;
    v.normal = (dot(blendedNrm, blendedNrm) > 1e-12f) ? normalize(blendedNrm) : v.normal;
    float3 blendedTan = v.tangent.xyz + deltaTan;
    v.tangent.xyz = (dot(blendedTan, blendedTan) > 1e-12f) ? normalize(blendedTan) : v.tangent.xyz;
    return v;
}

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID){
    uint vid = DTid.x;
    if (vid >= g_numVertices)
        return;

    Vertex v = morphVertex(vid);

    if (g_numJoints > 0){
        BoneWeight bw = boneWeights[vid];

        float4x4 skinModel =
            bw.weights.x * palette[(uint)bw.indices.x] +
            bw.weights.y * palette[(uint)bw.indices.y] +
            bw.weights.z * palette[(uint)bw.indices.z] +
            bw.weights.w * palette[(uint)bw.indices.w];

        float4x4 skinNrm =
            bw.weights.x * paletteNormal[(uint)bw.indices.x] +
            bw.weights.y * paletteNormal[(uint)bw.indices.y] +
            bw.weights.z * paletteNormal[(uint)bw.indices.z] +
            bw.weights.w * paletteNormal[(uint)bw.indices.w];

        v.position = mul(float4(v.position, 1.0f), skinModel).xyz;
        v.normal = normalize(mul(v.normal, (float3x3)skinNrm));
        v.tangent.xyz = normalize(mul(v.tangent.xyz, (float3x3)skinNrm));
    }

    outVertex[g_vertexOffset + vid] = v;
}
