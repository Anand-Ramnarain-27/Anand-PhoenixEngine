// Skinning.hlsl — GPU skinning compute shader
// Root signature (set by SkinningPass):
//   [0] b0 : root constant — { numVertices }
//   [1] t0 : StructuredBuffer<float4x4>   — palette      (model-space transforms)
//   [2] t1 : StructuredBuffer<float4x4>   — paletteNormal (inverse-transpose of palette, for normals)
//   [3] t2 : StructuredBuffer<Vertex>     — input vertices
//   [4] t3 : StructuredBuffer<BoneWeight> — per-vertex joint indices and weights
//   [5] u0 : RWStructuredBuffer<Vertex>   — output skinned vertices (written in world space)

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
    uint g_numVertices;
    uint g_paletteOffset;
    uint g_vertexOffset;
    uint g_pad;
};

StructuredBuffer<float4x4>   palette       : register(t0);
StructuredBuffer<float4x4>   paletteNormal : register(t1);
StructuredBuffer<Vertex>     inVertex      : register(t2);
StructuredBuffer<BoneWeight> boneWeights   : register(t3);
RWStructuredBuffer<Vertex>   outVertex     : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vid = DTid.x;
    if (vid >= g_numVertices)
        return;

    BoneWeight bw = boneWeights[vid];

    // Blend four skinning matrices for position transforms.
    float4x4 skinModel =
        bw.weights.x * palette[g_paletteOffset + (uint)bw.indices.x] +
        bw.weights.y * palette[g_paletteOffset + (uint)bw.indices.y] +
        bw.weights.z * palette[g_paletteOffset + (uint)bw.indices.z] +
        bw.weights.w * palette[g_paletteOffset + (uint)bw.indices.w];

    // Blend four inverse-transpose matrices for normal/tangent transforms.
    float4x4 skinNormal =
        bw.weights.x * paletteNormal[g_paletteOffset + (uint)bw.indices.x] +
        bw.weights.y * paletteNormal[g_paletteOffset + (uint)bw.indices.y] +
        bw.weights.z * paletteNormal[g_paletteOffset + (uint)bw.indices.z] +
        bw.weights.w * paletteNormal[g_paletteOffset + (uint)bw.indices.w];

    Vertex v = inVertex[vid];

    // Row-vector convention (DirectX): v * M applies M to v.
    float3 skinnedPos = mul(float4(v.position, 1.0f), skinModel).xyz;
    float3 skinnedNrm = normalize(mul(v.normal,      (float3x3)skinNormal));
    float3 skinnedTan = normalize(mul(v.tangent.xyz, (float3x3)skinNormal));

    Vertex o;
    o.position = skinnedPos;
    o.texCoord = v.texCoord;
    o.normal   = skinnedNrm;
    o.tangent  = float4(skinnedTan, v.tangent.w);

    outVertex[g_vertexOffset + vid] = o;
}
