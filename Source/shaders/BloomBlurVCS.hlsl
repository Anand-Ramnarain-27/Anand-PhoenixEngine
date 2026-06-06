// BloomBlurVCS — Vertical Gaussian blur, same groupshared LDS pattern as BloomBlurHCS.
// [numthreads(1, 128, 1)] with group count = (h + 127) / 128 × w-tiles.
// A UAV barrier is placed between the horizontal and this vertical pass (C++ side)
// so all horizontal writes are guaranteed visible before we read them here.

#define THREAD_COUNT  128
#define BLUR_RADIUS   7
#define CACHE_SIZE    (THREAD_COUNT + 2 * BLUR_RADIUS)   // 142

cbuffer CbBloom : register(b0)
{
    uint2 TextureSize;
    float Threshold;
    float Strength;
};

Texture2D<float4>   InputTex  : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

groupshared float4 gsCache[CACHE_SIZE];

float gauss(float x)
{
    static const float sigma = 3.0f;
    return exp(-(x * x) / (2.0f * sigma * sigma));
}

[numthreads(1, THREAD_COUNT, 1)]
void main(uint3 groupID    : SV_GroupID,
          uint  localIdx   : SV_GroupIndex,
          uint3 dispatchID : SV_DispatchThreadID)
{
    int baseY = (int)(groupID.y * THREAD_COUNT) - BLUR_RADIUS;
    int col   = (int)dispatchID.x;

    for (uint i = localIdx; i < CACHE_SIZE; i += THREAD_COUNT)
    {
        int sampleY = clamp(baseY + (int)i, 0, (int)TextureSize.y - 1);
        int2 coord  = int2(clamp(col, 0, (int)TextureSize.x - 1), sampleY);
        gsCache[i]  = InputTex[coord];
    }

    GroupMemoryBarrierWithGroupSync();

    if ((uint)col >= TextureSize.x || dispatchID.y >= TextureSize.y)
        return;

    float4 result = float4(0, 0, 0, 0);
    float  totalW = 0.0f;
    [unroll]
    for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
    {
        float  w  = gauss((float)k);
        result   += gsCache[localIdx + (uint)BLUR_RADIUS + k] * w;
        totalW   += w;
    }

    OutputTex[dispatchID.xy] = result / totalW;
}
