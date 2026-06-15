#pragma once
#include "Globals.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Noise {

constexpr float kTau = 6.283185307179586f;

inline float lerpf(float a, float b, float t){ return a + (b - a) * t; }

inline uint32_t hash(uint32_t x){
    x = (x ^ (x >> 16)) * 0x21f0aaadU;
    x = (x ^ (x >> 15)) * 0x735a2d97U;
    return x ^ (x >> 15);
}
inline uint32_t hash2(uint32_t x, uint32_t y){ return hash(x ^ hash(y)); }
inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t z){ return hash(x ^ hash2(y, z)); }

inline float hashToFloat01(uint32_t h){
    return float(h >> 8) * (1.0f / 16777216.0f);
}

inline float hermiteFade(float t){ return t * t * (3.0f - 2.0f * t); }
inline float quinticFade(float t){ return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

inline float valueNoise1D(float x){
    float i = std::floor(x);
    float f = x - i;
    float u = hermiteFade(f);
    float a = hashToFloat01(hash((uint32_t)(int64_t)i));
    float b = hashToFloat01(hash((uint32_t)(int64_t)i + 1u));
    return lerpf(a, b, u);
}

inline float grad1D(int32_t xi){ return hashToFloat01(hash((uint32_t)xi)) * 2.0f - 1.0f; }

inline Vector2 grad2D(int32_t xi, int32_t yi){
    float angle = hashToFloat01(hash2((uint32_t)xi, (uint32_t)yi)) * kTau;
    return Vector2(std::cos(angle), std::sin(angle));
}

inline Vector3 grad3D(int32_t xi, int32_t yi, int32_t zi){
    uint32_t h0 = hash3((uint32_t)xi, (uint32_t)yi, (uint32_t)zi);
    uint32_t h1 = hash(h0);
    float c = 2.0f * hashToFloat01(h0) - 1.0f;
    float s = std::sqrt(std::max(0.0f, 1.0f - c * c));
    float phi = kTau * hashToFloat01(h1);
    return Vector3(std::cos(phi) * s, std::sin(phi) * s, c);
}

inline float gradientNoise1D(float x){
    float i = std::floor(x);
    float f = x - i;
    int32_t ii = (int32_t)i;
    float ga = grad1D(ii);
    float gb = grad1D(ii + 1);
    return lerpf(ga * f, gb * (f - 1.0f), hermiteFade(f));
}

inline float gradientNoise2D(float x, float y){
    float ix = std::floor(x), iy = std::floor(y);
    float fx = x - ix, fy = y - iy;
    int32_t xi = (int32_t)ix, yi = (int32_t)iy;

    Vector2 ga = grad2D(xi, yi);
    Vector2 gb = grad2D(xi + 1, yi);
    Vector2 gc = grad2D(xi, yi + 1);
    Vector2 gd = grad2D(xi + 1, yi + 1);

    float va = ga.x * fx + ga.y * fy;
    float vb = gb.x * (fx - 1.f) + gb.y * fy;
    float vc = gc.x * fx + gc.y * (fy - 1.f);
    float vd = gd.x * (fx - 1.f) + gd.y * (fy - 1.f);

    float ux = hermiteFade(fx), uy = hermiteFade(fy);
    return lerpf(lerpf(va, vb, ux), lerpf(vc, vd, ux), uy);
}
inline float gradientNoise2D(const Vector2& p){ return gradientNoise2D(p.x, p.y); }

inline float gradientNoise3D(float x, float y, float z){
    float ix = std::floor(x), iy = std::floor(y), iz = std::floor(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    int32_t xi = (int32_t)ix, yi = (int32_t)iy, zi = (int32_t)iz;

    Vector3 g0 = grad3D(xi, yi, zi);
    Vector3 g1 = grad3D(xi + 1, yi, zi);
    Vector3 g2 = grad3D(xi, yi + 1, zi);
    Vector3 g3 = grad3D(xi + 1, yi + 1, zi);
    Vector3 g4 = grad3D(xi, yi, zi + 1);
    Vector3 g5 = grad3D(xi + 1, yi, zi + 1);
    Vector3 g6 = grad3D(xi, yi + 1, zi + 1);
    Vector3 g7 = grad3D(xi + 1, yi + 1, zi + 1);

    auto d = [](const Vector3& g, float dx, float dy, float dz){ return g.x * dx + g.y * dy + g.z * dz; };
    float v0 = d(g0, fx, fy, fz);
    float v1 = d(g1, fx - 1.f, fy, fz);
    float v2 = d(g2, fx, fy - 1.f, fz);
    float v3 = d(g3, fx - 1.f, fy - 1.f, fz);
    float v4 = d(g4, fx, fy, fz - 1.f);
    float v5 = d(g5, fx - 1.f, fy, fz - 1.f);
    float v6 = d(g6, fx, fy - 1.f, fz - 1.f);
    float v7 = d(g7, fx - 1.f, fy - 1.f, fz - 1.f);

    float ux = quinticFade(fx), uy = quinticFade(fy), uz = quinticFade(fz);
    float front = lerpf(lerpf(v0, v1, ux), lerpf(v2, v3, ux), uy);
    float back = lerpf(lerpf(v4, v5, ux), lerpf(v6, v7, ux), uy);
    return lerpf(front, back, uz);
}
inline float gradientNoise3D(const Vector3& p){ return gradientNoise3D(p.x, p.y, p.z); }

inline float fbm3D(const Vector3& p, int octaves = 5, float frequency = 0.1f, float amplitude = 0.5f){
    float value = 0.0f;
    for (int i = 0; i < octaves; ++i){
        value += amplitude * gradientNoise3D(p * frequency);
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

inline float noiseToAngle(float n){
    return std::clamp(n * 0.5f + 0.5f, 0.0f, 1.0f) * kTau;
}

}
