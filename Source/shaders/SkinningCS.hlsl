// SkinningCS.hlsl — GPU skinning compute shader
// Reads original vertex data + bone weights + matrix palette and writes skinned vertices.
//
// Root signature (set by SkinningPass):
//   [0] b0 : root constants — { vertexCount, paletteOffset, vertexOffset, pad }
//   [1] t0 : StructuredBuffer<Vertex>     — original vertex positions/normals
//   [2] t1 : StructuredBuffer<BoneWeight> — per-vertex joint indices and weights
//   [3] t2 : StructuredBuffer<float4x4>  — combined matrix palette for all skins
//   [4] u0 : RWStructuredBuffer<Vertex>  — output skinned vertex buffer

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

cbuffer SkinCB : register(b0)
{
    uint g_vertexCount;
    uint g_paletteOffset; // base joint index in the combined palette for this skin
    uint g_vertexOffset;  // base vertex index in the combined output buffer
    uint g_pad;
};

StructuredBuffer<Vertex>     InputVertices : register(t0);
StructuredBuffer<BoneWeight> BoneWeights   : register(t1);
StructuredBuffer<float4x4>   Palette       : register(t2);
RWStructuredBuffer<Vertex>   Output        : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vid = DTid.x;
    if (vid >= g_vertexCount)
        return;

    BoneWeight bw = BoneWeights[vid];

    // Blend skinning matrices weighted by bone weights.
    // Palette indices are relative to g_paletteOffset (joint 0 of this skin).
    float4x4 skin =
        bw.weights.x * Palette[g_paletteOffset + (uint)bw.indices.x] +
        bw.weights.y * Palette[g_paletteOffset + (uint)bw.indices.y] +
        bw.weights.z * Palette[g_paletteOffset + (uint)bw.indices.z] +
        bw.weights.w * Palette[g_paletteOffset + (uint)bw.indices.w];

    Vertex v = InputVertices[vid];

    // Row-vector convention (DirectX): v * M applies M to v.
    float4 skinnedPos = mul(float4(v.position, 1.0f), skin);
    float3 skinnedNrm = normalize(mul(v.normal,      (float3x3)skin));
    float3 skinnedTan = normalize(mul(v.tangent.xyz, (float3x3)skin));

    Vertex out_v;
    out_v.position = skinnedPos.xyz;
    out_v.texCoord = v.texCoord;       // UV unchanged
    out_v.normal   = skinnedNrm;
    out_v.tangent  = float4(skinnedTan, v.tangent.w);

    Output[g_vertexOffset + vid] = out_v;
}
