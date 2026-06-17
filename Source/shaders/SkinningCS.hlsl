
#include "SkinVertex.hlsli"

cbuffer SkinCB : register(b0){
    uint g_vertexCount;
    uint g_paletteOffset;
    uint g_vertexOffset;
    uint g_pad;
};

StructuredBuffer<Vertex> InputVertices : register(t0);
StructuredBuffer<BoneWeight> BoneWeights : register(t1);
StructuredBuffer<float4x4> Palette : register(t2);
RWStructuredBuffer<Vertex> Output : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID){
    uint vid = DTid.x;
    if (vid >= g_vertexCount)
        return;

    BoneWeight bw = BoneWeights[vid];

    float4x4 skin =
        bw.weights.x * Palette[g_paletteOffset + (uint)bw.indices.x] +
        bw.weights.y * Palette[g_paletteOffset + (uint)bw.indices.y] +
        bw.weights.z * Palette[g_paletteOffset + (uint)bw.indices.z] +
        bw.weights.w * Palette[g_paletteOffset + (uint)bw.indices.w];

    Vertex v = InputVertices[vid];

    float4 skinnedPos = mul(float4(v.position, 1.0f), skin);
    float3 skinnedNrm = normalize(mul(v.normal, (float3x3)skin));
    float3 skinnedTan = normalize(mul(v.tangent.xyz, (float3x3)skin));

    Vertex out_v;
    out_v.position = skinnedPos.xyz;
    out_v.texCoord = v.texCoord;
    out_v.normal = skinnedNrm;
    out_v.tangent = float4(skinnedTan, v.tangent.w);

    Output[g_vertexOffset + vid] = out_v;
}
