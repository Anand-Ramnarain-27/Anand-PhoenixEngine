// BloomBlurHCS — Lecture 14 (GPU Architecture): groupshared LDS cache, SIMT lockstep.
// Horizontal Gaussian blur using groupshared memory to avoid repeated global memory reads.
// Each thread group (128 × 1) loads a stripe of texels into fast CU-local shared memory,
// then all threads read from that cache instead of from slow global VRAM.
//
// Key concepts demonstrated:
//   - groupshared (backed by CU local memory — 48–160 KB on hardware)
//   - GroupMemoryBarrierWithGroupSync() — all threads sync before reading the cache
//   - Bounds check after sync so every thread still reaches the barrier
//   - [numthreads(128, 1, 1)] with group count = (w + 127) / 128

#define THREAD_COUNT  128
#define BLUR_RADIUS   7
#define CACHE_SIZE    (THREAD_COUNT + 2 * BLUR_RADIUS)   // 142 entries

cbuffer CbBloom : register(b0)
{
    uint2 TextureSize;
    float Threshold;
    float Strength;
};

Texture2D<float4>   InputTex  : register(t0);   // previous pass output (SRV)
RWTexture2D<float4> OutputTex : register(u0);   // horizontal-blurred output (UAV)

// Groupshared / LDS — allocated in the CU's local memory.
// Visible to ALL 128 threads in this group, but NOT across groups.
// Size: 142 × 16 bytes = 2272 bytes (well within the 48 KB hardware limit).
groupshared float4 gsCache[CACHE_SIZE];

// Gaussian weight (sigma = 3.0 gives a smooth 15-tap kernel)
float gauss(float x)
{
    static const float sigma = 3.0f;
    return exp(-(x * x) / (2.0f * sigma * sigma));
}

[numthreads(THREAD_COUNT, 1, 1)]
void main(uint3 groupID    : SV_GroupID,
          uint  localIdx   : SV_GroupIndex,
          uint3 dispatchID : SV_DispatchThreadID)
{
    // Each group covers THREAD_COUNT output texels, but needs BLUR_RADIUS extra on each side.
    // baseX is the first texel this group must sample (may be negative → clamped below).
    int baseX = (int)(groupID.x * THREAD_COUNT) - BLUR_RADIUS;
    int row   = (int)dispatchID.y;

    // Cooperatively load CACHE_SIZE texels into groupshared memory.
    // With 128 threads and 142 entries, the first 14 threads each execute this loop twice.
    for (uint i = localIdx; i < CACHE_SIZE; i += THREAD_COUNT)
    {
        int sampleX = clamp(baseX + (int)i, 0, (int)TextureSize.x - 1);
        int2 coord  = int2(sampleX, clamp(row, 0, (int)TextureSize.y - 1));
        gsCache[i]  = InputTex[coord];
    }

    // BARRIER: all threads must finish writing gsCache before any thread reads from it.
    // This is GroupMemoryBarrierWithGroupSync (not DeviceMemoryBarrier — no UAV access here).
    GroupMemoryBarrierWithGroupSync();

    // Bounds check AFTER the barrier so the barrier is not skipped.
    if (dispatchID.x >= TextureSize.x || (uint)row >= TextureSize.y)
        return;

    // Apply 15-tap Gaussian using the LDS cache — zero global memory reads here.
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
