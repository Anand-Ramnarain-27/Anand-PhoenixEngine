// MeshVS.hlsl

// Slot 0: view-projection matrix (16 x 32-bit constants)
cbuffer VP : register(b0)
{
    float4x4 viewProj;
};

// Slot 1: per-object world matrix (16 x 32-bit constants)
cbuffer World : register(b1)
{
    float4x4 world;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 nrm : NORMAL;
    float4 tangent : TANGENT; // w = bitangent sign (-1 or +1)
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
    float4 tangent : TANGENT;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.worldPos = worldPos.xyz;
    output.pos = mul(worldPos, viewProj);
    output.uv = input.uv;

    // Use the upper-left 3x3 of world for normals.
    // This is correct for uniform-scale objects (which is the common case).
    // For non-uniform scale the CPU side should pass an inverse-transpose
    // normal matrix in a separate constant buffer - see MeshPipeline notes.
    float3x3 worldRot = (float3x3) world;
    output.nrm = normalize(mul(input.nrm, worldRot));
    output.tangent = float4(normalize(mul(input.tangent.xyz, worldRot)),
                               input.tangent.w); // preserve bitangent sign

    return output;
}
