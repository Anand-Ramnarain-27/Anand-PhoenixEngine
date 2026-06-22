
Texture2D<float> DepthIn : register(t0);
RWTexture2D<float2> MinMaxOut : register(u0);

cbuffer ReduceCB : register(b0){
    uint2 SrcSize;
    uint2 DstSize;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID){
    if (id.x >= DstSize.x || id.y >= DstSize.y) return;

    float mn = 1.0f;
    float mx = 0.0f;
    for (uint y = 0; y < 8; ++y){
        for (uint x = 0; x < 8; ++x){
            uint2 s = id.xy * 8 + uint2(x, y);
            if (s.x >= SrcSize.x || s.y >= SrcSize.y) continue;
            float d = DepthIn.Load(int3(s, 0));
            if (d < 1.0f) mn = min(mn, d);
            mx = max(mx, d);
        }
    }
    if (mn > mx) mn = mx;
    MinMaxOut[id.xy] = float2(mn, mx);
}
