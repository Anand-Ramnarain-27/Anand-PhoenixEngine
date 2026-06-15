struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vid : SV_VertexID){
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2( 3.0f, -1.0f)
    };

    VSOutput o;
    o.pos = float4(positions[vid], 0.0f, 1.0f);
    o.uv = positions[vid] * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    return o;
}
