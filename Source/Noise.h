#pragma once
// Hash-based gradient noise and fBm on the CPU (particle turbulence). Deterministic: same input, same output.

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

inline float quinticFade(float t){ return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

inline Vector3 grad3D(int32_t xi, int32_t yi, int32_t zi){
    uint32_t h0 = hash3((uint32_t)xi, (uint32_t)yi, (uint32_t)zi);
    uint32_t h1 = hash(h0);
    float c = 2.0f * hashToFloat01(h0) - 1.0f;
    float s = std::sqrt(std::max(0.0f, 1.0f - c * c));
    float phi = kTau * hashToFloat01(h1);
    return Vector3(std::cos(phi) * s, std::sin(phi) * s, c);
}

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

/// Fractal sum of `octaves` noise layers, each at twice the frequency and half the amplitude of the last.
inline float fbm3D(const Vector3& p, int octaves = 5, float frequency = 0.1f, float amplitude = 0.5f){
    float value = 0.0f;
    for (int i = 0; i < octaves; ++i){
        value += amplitude * gradientNoise3D(p * frequency);
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

/// Maps a noise value (about -1..1) to an angle 0..2pi.
inline float noiseToAngle(float n){
    return std::clamp(n * 0.5f + 0.5f, 0.0f, 1.0f) * kTau;
}

} // namespace Noise
