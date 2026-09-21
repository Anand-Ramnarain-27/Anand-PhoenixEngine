#include "Globals.h"
#include "RayMath.h"
#include <algorithm>
#include <cmath>

namespace RayMath {

    float RayVsAABB(const Ray& ray, const Vector3& mn, const Vector3& mx, float maxDist){
        float tmin = 0.f, tmax = FLT_MAX;
        const float* ro = &ray.origin.x;
        const float* rd = &ray.direction.x;
        const float* lo = &mn.x;
        const float* hi = &mx.x;

        for (int i = 0; i < 3; ++i){
            if (std::abs(rd[i]) < 1e-8f){
                if (ro[i] < lo[i] || ro[i] > hi[i]) return FLT_MAX;
            } else {
                float invD = 1.f / rd[i];
                float t1 = (lo[i] - ro[i]) * invD;
                float t2 = (hi[i] - ro[i]) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = tmin > t1 ? tmin : t1;
                tmax = tmax < t2 ? tmax : t2;
                if (tmin > tmax) return FLT_MAX;
            }
        }
        float t = tmin >= 0.f ? tmin : (tmax >= 0.f ? tmax : FLT_MAX);
        return (t <= maxDist) ? t : FLT_MAX;
    }

    float RayVsSphere(const Ray& ray, const Vector3& center, float radius, float maxDist){
        Vector3 oc = ray.origin - center;
        float b = oc.Dot(ray.direction);
        float c = oc.LengthSquared() - radius * radius;
        float disc = b * b - c;
        if (disc < 0.f) return FLT_MAX;

        float sqrtDisc = sqrtf(disc);
        float t0 = -b - sqrtDisc;
        float t1 = -b + sqrtDisc;
        float t = t0 >= 0.f ? t0 : t1;
        return (t >= 0.f && t <= maxDist) ? t : FLT_MAX;
    }

    float RayVsOBB(const Ray& ray, const Vector3& center, const Vector3 axes[3], const float halves[3],
                   float maxDist, Vector3* outNormal){
        Vector3 d = ray.origin - center;
        Vector3 originLocal(d.Dot(axes[0]), d.Dot(axes[1]), d.Dot(axes[2]));
        Vector3 dirLocal(ray.direction.Dot(axes[0]), ray.direction.Dot(axes[1]), ray.direction.Dot(axes[2]));

        Ray localRay{ originLocal, dirLocal };
        Vector3 mn(-halves[0], -halves[1], -halves[2]);
        Vector3 mx(halves[0], halves[1], halves[2]);
        float t = RayVsAABB(localRay, mn, mx, maxDist);
        if (t == FLT_MAX) return FLT_MAX;

        if (outNormal){
            Vector3 hitLocal = originLocal + dirLocal * t;
            int bestAxis = 0;
            float bestDist = FLT_MAX;
            float sign = 1.f;
            const float* hl = &hitLocal.x;
            for (int i = 0; i < 3; ++i){
                float distToPos = std::abs(halves[i] - hl[i]);
                float distToNeg = std::abs(-halves[i] - hl[i]);
                if (distToPos < bestDist){ bestDist = distToPos; bestAxis = i; sign = 1.f; }
                if (distToNeg < bestDist){ bestDist = distToNeg; bestAxis = i; sign = -1.f; }
            }
            *outNormal = axes[bestAxis] * sign;
        }
        return t;
    }

    float RayVsTriangle(const Ray& ray, const Vector3& v0, const Vector3& v1, const Vector3& v2){
        constexpr float kEps = 1e-8f;

        Vector3 e1 = v1 - v0;
        Vector3 e2 = v2 - v0;
        Vector3 h = ray.direction.Cross(e2);
        float a = e1.Dot(h);

        if (std::abs(a) < kEps) return FLT_MAX;

        float f = 1.f / a;
        Vector3 s = ray.origin - v0;
        float u = f * s.Dot(h);
        if (u < 0.f || u > 1.f) return FLT_MAX;

        Vector3 q = s.Cross(e1);
        float v = f * ray.direction.Dot(q);
        if (v < 0.f || u + v > 1.f) return FLT_MAX;

        float t = f * e2.Dot(q);
        return t > kEps ? t : FLT_MAX;
    }
}
