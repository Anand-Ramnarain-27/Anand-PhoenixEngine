
cbuffer BlurData : register(b0){
    int2 Direction;
    int2 Size;
};

Texture2DArray<float2> InputTex : register(t0);
RWTexture2DArray<float2> OutputTex : register(u0);
SamplerState LinearClampS : register(s0);

static const int SAMPLE_COUNT = 3;
static const float OFFSETS[SAMPLE_COUNT] = { -1.3446745, 0.4466723, 2.0 };
static const float WEIGHTS[SAMPLE_COUNT] = { 0.3556437, 0.5217749, 0.1225813 };

[numthreads(8, 8, 1)]
void main(uint3 globalIdx : SV_DispatchThreadID){
    if (globalIdx.x >= (uint)Size.x || globalIdx.y >= (uint)Size.y) return;
    const uint slice = globalIdx.z;

    float2 invSize = 1.0 / float2(Size);
    float2 result = 0.0;
    for (int i = 0; i < SAMPLE_COUNT; ++i){
        float2 sampleIdx = float2(globalIdx.xy) + float2(Direction) * OFFSETS[i];
        float2 uv = (sampleIdx + 0.5) * invSize;
        result += InputTex.SampleLevel(LinearClampS, float3(uv, slice), 0) * WEIGHTS[i];
    }
    OutputTex[uint3(globalIdx.xy, slice)] = result;
}
