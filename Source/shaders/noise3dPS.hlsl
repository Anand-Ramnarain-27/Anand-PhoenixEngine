
cbuffer cbNoisePerFrame : register(b0){
    float4x4 matVP;
    float4x4 matGeo;
    float uTime;
    float3 _pad;
};

struct PSInput {
    float4 svPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};


uint hashU(uint x){
    x = (x ^ (x >> 16)) * 0x21f0aaadU;
    x = (x ^ (x >> 15)) * 0x735a2d97U;
    return x ^ (x >> 15);
}
uint hash2U(uint2 v){ return hashU(v.x ^ hashU(v.y)); }
uint hash3U(uint3 v){ return hashU(v.x ^ hash2U(v.yz)); }

float hash2Float(uint h){
    return (float)(h >> 8) * asfloat(0x33800000);
}

float3 grad(uint3 cell){
    uint h0 = hash3U(cell);
    uint h1 = hashU(h0);
    float ct = 2.0f * hash2Float(h0) - 1.0f;
    float st = sqrt(max(0.0f, 1.0f - ct * ct));
    float ph = 6.28318530718f * hash2Float(h1);
    return float3(cos(ph) * st, sin(ph) * st, ct);
}

float gradientNoise(float3 p){
    int3 cell = (int3)floor(p);
    float3 f = frac(p);
    float3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float3 g0 = grad(uint3(cell + int3(0,0,0)));
    float3 g1 = grad(uint3(cell + int3(1,0,0)));
    float3 g2 = grad(uint3(cell + int3(0,1,0)));
    float3 g3 = grad(uint3(cell + int3(1,1,0)));
    float3 g4 = grad(uint3(cell + int3(0,0,1)));
    float3 g5 = grad(uint3(cell + int3(1,0,1)));
    float3 g6 = grad(uint3(cell + int3(0,1,1)));
    float3 g7 = grad(uint3(cell + int3(1,1,1)));

    float v0 = dot(g0, f - float3(0,0,0));
    float v1 = dot(g1, f - float3(1,0,0));
    float v2 = dot(g2, f - float3(0,1,0));
    float v3 = dot(g3, f - float3(1,1,0));
    float v4 = dot(g4, f - float3(0,0,1));
    float v5 = dot(g5, f - float3(1,0,1));
    float v6 = dot(g6, f - float3(0,1,1));
    float v7 = dot(g7, f - float3(1,1,1));

    float front = lerp(lerp(v0, v1, u.x), lerp(v2, v3, u.x), u.y);
    float back = lerp(lerp(v4, v5, u.x), lerp(v6, v7, u.x), u.y);
    return lerp(front, back, u.z);
}

float fbm(float3 st){
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 0.1f;
    [unroll]
    for (int i = 0; i < 8; ++i){
        value += amplitude * (gradientNoise(st * frequency) + 0.5f);
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

float4 main(PSInput pin) : SV_TARGET {
    float res = fbm(pin.WorldPos * 24.0f + uTime);
    return float4(res, res, res, 1.0f);
}
