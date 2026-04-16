struct Vertex
{
    float3 position;
    float2 uv;
    float3 normal;
    float4 tangent;
    uint4 joints; // matches C++ Vertex layout
    float4 weights;
};
 
struct BoneWeight
{
    uint4 indices; // bone indices (same as joints in Vertex)
    float4 weights;
};
 
// Constant buffer: vertex count, joint count, morph target count
cbuffer SkinConstants : register(b0)
{
    uint numVertices;
    uint numJoints;
    uint numMorphTargets; // Phase 3 — 0 for now
    uint _pad;
};
 
// Slide 29-30: palettes — B^-1 * T precomputed on CPU
StructuredBuffer<float4x4> paletteModel : register(t0);
StructuredBuffer<float4x4> paletteNormal : register(t1);
StructuredBuffer<Vertex> inVertex : register(t2);
StructuredBuffer<Vertex> morphVertices : register(t3); // Phase 3
StructuredBuffer<float> morphWeights : register(t4); // Phase 3
 
RWStructuredBuffer<Vertex> outVertex : register(u0);
 
// Phase 3 morph equation (slide 12 from Animation III).
// If numMorphTargets == 0 just returns the T-pose vertex.
Vertex morphVertex(uint index)
{
    Vertex v = inVertex[index];
    for (uint i = 0; i < numMorphTargets; ++i)
    {
        Vertex delta = morphVertices[i * numVertices + index];
        v.position += delta.position * morphWeights[i];
        v.normal += delta.normal * morphWeights[i];
        v.tangent.xyz += delta.tangent.xyz * morphWeights[i];
    }
    if (numMorphTargets > 0)
    {
        v.normal = normalize(v.normal);
        v.tangent.xyz = normalize(v.tangent.xyz);
    }
    return v;
}
 
[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint threadIndex = tid.x;
    if (threadIndex >= numVertices)
        return;
 
    // Step 1: apply morph targets (or just get T-pose vertex)
    Vertex v = morphVertex(threadIndex);
 
    // Step 2: Slide 31 — apply skinning if there are joints
    if (numJoints > 0)
    {
        uint4 idx = v.joints;
        float4 w = v.weights;
 
        // Build blended skinning matrix (slide 31)
        float4x4 skinModel = paletteModel[idx[0]] * w[0]
                            + paletteModel[idx[1]] * w[1]
                            + paletteModel[idx[2]] * w[2]
                            + paletteModel[idx[3]] * w[3];
        float4x4 skinNormal = paletteNormal[idx[0]] * w[0]
                            + paletteNormal[idx[1]] * w[1]
                            + paletteNormal[idx[2]] * w[2]
                            + paletteNormal[idx[3]] * w[3];
 
        // p' = p * B^-1 * T * w   (slide 10)
        v.position = mul(float4(v.position, 1.0f), skinModel).xyz;
        v.normal = mul(float4(v.normal, 0.0f), skinNormal).xyz;
        v.tangent.xyz = mul(float4(v.tangent.xyz, 0.0f), skinNormal).xyz;
    }
 
    outVertex[threadIndex] = v;
}
