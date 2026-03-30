// Skinning.hlsl
// Compile: fxc /T cs_5_1 /E main /Fo Skinning.cso Skinning.hlsl
// Or via your existing shader build step.
 
struct Vertex
{
    float3 position;
    float2 uv;
    float3 normal;
    float4 tangent; // xyz=tangent, w=handedness
};
 
struct BoneWeight
{
    uint4 indices;
    float4 weights;
};
 
// Root constants — set via SetComputeRoot32BitConstants
cbuffer SkinConstants : register(b0)
{
    uint numVertices;
    uint numJoints; // 0 = morph-only, skip skinning
    uint numMorphTargets; // 0 = skeletal-only, skip morphing
    uint _pad;
};
 
// SRVs — set via SetComputeRootShaderResourceView
StructuredBuffer<float4x4> paletteModel : register(t0); // Bj_inv * Tj transposed
StructuredBuffer<float4x4> paletteNormal : register(t1); // inverse-transpose normals
StructuredBuffer<Vertex> inVertex : register(t2); // T-pose bind-pose verts
StructuredBuffer<BoneWeight> boneWeights : register(t3); // per vertex up to 4 bones
StructuredBuffer<Vertex> morphVerts : register(t4); // [target * numVerts + vi]
StructuredBuffer<float> morphWeights : register(t5); // [numTargets] per instance
 
// UAV — set via SetComputeRootUnorderedAccessView
RWStructuredBuffer<Vertex> outVertex : register(u0);
 
// Apply morph target deltas to the neutral vertex.
Vertex morphVertex(uint idx)
{
    Vertex v = inVertex[idx];
    for (uint m = 0; m < numMorphTargets; ++m)
    {
        float w = morphWeights[m];
        if (w == 0.0f)
            continue;
        Vertex d = morphVerts[m * numVertices + idx];
        v.position += d.position * w;
        v.normal += d.normal * w;
        v.tangent.xyz += d.tangent.xyz * w;
    }
    v.normal = normalize(v.normal);
    v.tangent = float4(normalize(v.tangent.xyz), v.tangent.w);
    return v;
}
 
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= numVertices)
        return;
 
    // Step 1: apply morph targets (if any)
    Vertex v = (numMorphTargets > 0) ? morphVertex(id.x) : inVertex[id.x];
 
    // Step 2: apply skinning (if any)
    if (numJoints > 0)
    {
        BoneWeight bw = boneWeights[id.x];
 
        // Blend model matrix
        float4x4 sm =
            paletteModel[bw.indices.x] * bw.weights.x +
            paletteModel[bw.indices.y] * bw.weights.y +
            paletteModel[bw.indices.z] * bw.weights.z +
            paletteModel[bw.indices.w] * bw.weights.w;
 
        // Blend normal matrix (inverse-transpose of model)
        float4x4 sn =
            paletteNormal[bw.indices.x] * bw.weights.x +
            paletteNormal[bw.indices.y] * bw.weights.y +
            paletteNormal[bw.indices.z] * bw.weights.z +
            paletteNormal[bw.indices.w] * bw.weights.w;
 
        v.position = mul(float4(v.position, 1.f), sm).xyz;
        v.normal = mul(float4(v.normal, 0.f), sn).xyz;
        v.tangent = float4(mul(float4(v.tangent.xyz, 0.f), sn).xyz,
                             v.tangent.w);
    }
 
    outVertex[id.x] = v;
}
