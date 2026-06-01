// Skinning.hlsl — GPU skinning + morph-target compute shader
// Fixed 2026-06-01: removed double-offset bug — palette SRVs are pre-offset by
// SkinningPass::dispatch() to this job's base joint, so indices must NOT add g_paletteOffset.
// Root signature (set by SkinningPass):
//   [0] b0 : root constants   { numVertices, paletteOffset, vertexOffset, numMorphTargets, numJoints }
//   [1] t0 : StructuredBuffer<float4x4>    — palette      (SRV already offset to this skin's base joint)
//   [2] t1 : StructuredBuffer<float4x4>    — paletteNormal (SRV already offset to this skin's base joint)
//   [3] t2 : StructuredBuffer<Vertex>      — input vertices (T-pose)
//   [4] t3 : StructuredBuffer<BoneWeight>  — per-vertex joint indices and weights
//   [5] u0 : RWStructuredBuffer<Vertex>    — output buffer (world space)
//   [6] t4 : StructuredBuffer<MorphVertex> — packed morph target deltas
//   [7] t5 : StructuredBuffer<float>       — per-target blend weights

struct Vertex
{
    float3 position;
    float2 texCoord;
    float3 normal;
    float4 tangent;
};

struct BoneWeight
{
    int4   indices;
    float4 weights;
};

// Matches Mesh::MorphVertex (C++ side): three float3 deltas, each padded to float4.
struct MorphVertex
{
    float3 deltaPosition; float _pad0;
    float3 deltaNormal;   float _pad1;
    float3 deltaTangent;  float _pad2;
};

cbuffer SkinCB : register(b0)
{
    uint g_numVertices;
    uint g_paletteOffset;   // unused for indexing — SRV binding already pre-offsets to this skin's base joint
    uint g_vertexOffset;    // base vertex index in the combined output buffer
    uint g_numMorphTargets;
    uint g_numJoints;
};

StructuredBuffer<float4x4>    palette        : register(t0);
StructuredBuffer<float4x4>    paletteNormal  : register(t1);
StructuredBuffer<Vertex>      inVertex       : register(t2);
StructuredBuffer<BoneWeight>  boneWeights    : register(t3);
StructuredBuffer<MorphVertex> morphVertices  : register(t4);
StructuredBuffer<float>       morphWeights   : register(t5);
RWStructuredBuffer<Vertex>    outVertex      : register(u0);

// Applies all morph targets to vertex 'index' and returns the blended T-pose vertex.
// Buffer layout: [Target0_V0 | Target0_V1 | ... | Target1_V0 | ...]
// Access:        morphVertices[targetIndex * g_numVertices + vertexIndex]
// When g_numMorphTargets == 0 the original T-pose vertex is returned unchanged.
Vertex morphVertex(uint index)
{
    Vertex v = inVertex[index];
    if (g_numMorphTargets == 0)
        return v;

    float3 deltaPos = (float3)0;
    float3 deltaNrm = (float3)0;
    float3 deltaTan = (float3)0;

    for (uint i = 0; i < g_numMorphTargets; ++i)
    {
        float       w  = morphWeights[i];
        MorphVertex mv = morphVertices[i * g_numVertices + index];
        deltaPos += w * mv.deltaPosition;
        deltaNrm += w * mv.deltaNormal;
        deltaTan += w * mv.deltaTangent;
    }

    v.position += deltaPos;
    // Guard: if the accumulated delta is too large, infinite, or NaN, fall back to T-pose position.
    // Threshold sqrt(1e6) ≈ 1000 units per component — catches garbage morph data before it
    // displaces vertices outside the frustum without blocking legitimate large-scale morphs.
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
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vid = DTid.x;
    if (vid >= g_numVertices)
        return;

    // Apply morph targets first (on T-pose), then skin.
    Vertex v = morphVertex(vid);

    if (g_numJoints > 0)
    {
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

        v.position    = mul(float4(v.position, 1.0f), skinModel).xyz;
        v.normal      = normalize(mul(v.normal,      (float3x3)skinNrm));
        v.tangent.xyz = normalize(mul(v.tangent.xyz, (float3x3)skinNrm));
    }

    outVertex[g_vertexOffset + vid] = v;
}
