struct Vertex
{
    float3 position;
    float2 uv;
    float3 normal;
    float4 tangent;
    uint4 joints; 
    float4 weights;
};
 
struct BoneWeight
{
    uint4 indices; 
    float4 weights;
};
 
cbuffer SkinConstants : register(b0)
{
    uint numVertices;
    uint numJoints;
    uint numMorphTargets; 
    uint _pad;
};
 
StructuredBuffer<float4x4> paletteModel : register(t0);
StructuredBuffer<float4x4> paletteNormal : register(t1);
StructuredBuffer<Vertex> inVertex : register(t2);
StructuredBuffer<Vertex> morphVertices : register(t3); 
StructuredBuffer<float> morphWeights : register(t4); 
 
RWStructuredBuffer<Vertex> outVertex : register(u0);
 
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
 
    Vertex v = morphVertex(threadIndex);
 
    if (numJoints > 0)
    {
        uint4 idx = v.joints;
        float4 w = v.weights;
 
        float4x4 skinModel = paletteModel[idx[0]] * w[0]
                            + paletteModel[idx[1]] * w[1]
                            + paletteModel[idx[2]] * w[2]
                            + paletteModel[idx[3]] * w[3];
        float4x4 skinNormal = paletteNormal[idx[0]] * w[0]
                            + paletteNormal[idx[1]] * w[1]
                            + paletteNormal[idx[2]] * w[2]
                            + paletteNormal[idx[3]] * w[3];
 
        v.position = mul(float4(v.position, 1.0f), skinModel).xyz;
        v.normal = mul(float4(v.normal, 0.0f), skinNormal).xyz;
        v.tangent.xyz = mul(float4(v.tangent.xyz, 0.0f), skinNormal).xyz;
    }
 
    outVertex[threadIndex] = v;
}
