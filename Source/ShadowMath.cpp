#include "Globals.h"
#include "ShadowMath.h"
#include <algorithm>
#include <cmath>

namespace ShadowMath {

void ExtractNearFar(const Matrix& proj, float& outNear, float& outFar){
    const float m33 = proj._33;
    const float m43 = proj._43;
    if (fabsf(m33) < 1e-6f || fabsf(m33 + 1.0f) < 1e-6f){
        outNear = 0.1f; outFar = 500.0f; return;
    }
    outNear = m43 / m33;
    outFar  = m43 / (m33 + 1.0f);
}

Matrix SpotLightViewProj(const Vector3& pos, const Vector3& dir,
                         float outerAngleRad, float range){
    Vector3 d = dir;
    if (d.LengthSquared() < 1e-8f) d = Vector3(0, -1, 0);
    d.Normalize();
    Vector3 up = (fabsf(d.Dot(Vector3::UnitY)) > 0.99f) ? Vector3::UnitZ : Vector3::UnitY;
    Matrix view = Matrix::CreateLookAt(pos, pos + d, up);
    float fov = std::min(2.0f * outerAngleRad, 3.10f);
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(fov, 1.0f, 0.05f, std::max(range, 0.1f));
    return view * proj;
}

void PointLightFaceViewProj(const Vector3& pos, float nearDist, float range,
                            Matrix out[6]){
    const Vector3 dirs[6] = {
        Vector3( 1, 0, 0), Vector3(-1, 0, 0),
        Vector3( 0, 1, 0), Vector3( 0,-1, 0),
        Vector3( 0, 0, 1), Vector3( 0, 0,-1),
    };
    const Vector3 ups[6] = {
        Vector3(0, 1, 0), Vector3(0, 1, 0),
        Vector3(0, 0,-1), Vector3(0, 0, 1),
        Vector3(0, 1, 0), Vector3(0, 1, 0),
    };
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(1.5707963f, 1.0f,
                                                       std::max(nearDist, 0.02f),
                                                       std::max(range, 0.1f));
    for (int i = 0; i < 6; ++i)
        out[i] = Matrix::CreateLookAt(pos, pos + dirs[i], ups[i]) * proj;
}

void CascadeSplits(float nearDist, float farDist, int count, float lambda,
                   float* outSplits){
    if (count < 1) count = 1;
    if (count > kMaxCascades) count = kMaxCascades;
    const float range = farDist - nearDist;
    const float ratio = farDist / std::max(nearDist, 1e-4f);
    for (int i = 1; i <= count; ++i){
        const float p = (float)i / (float)count;
        const float logSplit = nearDist * powf(ratio, p);
        const float linSplit = nearDist + range * p;
        outSplits[i - 1] = linSplit + lambda * (logSplit - linSplit);
    }
    outSplits[count - 1] = farDist;
}

DirShadowResult DirectionalLightViewProj(const Matrix& camView,
                                         const Matrix& camProj,
                                         const Vector3& lightDir,
                                         float nearDist, float farDist,
                                         float sunDistance,
                                         uint32_t shadowResolution){
    DirShadowResult r{};

    Matrix invView;
    camView.Invert(invView);

    const float tanH = (fabsf(camProj._11) > 1e-6f) ? 1.0f / camProj._11 : 1.0f;
    const float tanV = (fabsf(camProj._22) > 1e-6f) ? 1.0f / camProj._22 : 1.0f;

    Vector3 corners[8];
    int idx = 0;
    const float dists[2] = { nearDist, farDist };
    for (int di = 0; di < 2; ++di){
        const float d = dists[di];
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sx = -1; sx <= 1; sx += 2)
                corners[idx++] = Vector3(sx * d * tanH, sy * d * tanV, -d);
    }
    for (auto& c : corners) c = Vector3::Transform(c, invView);

    Vector3 center(0, 0, 0);
    for (const auto& c : corners) center += c;
    center /= 8.0f;
    float radius = 0.0f;
    for (const auto& c : corners) radius = std::max(radius, (c - center).Length());
    radius = ceilf(radius * 16.0f) / 16.0f;

    Vector3 dir = lightDir;
    if (dir.LengthSquared() < 1e-8f) dir = Vector3(0, -1, 0);
    dir.Normalize();

    Vector3 up = (fabsf(dir.Dot(Vector3::UnitY)) > 0.99f) ? Vector3::UnitZ : Vector3::UnitY;

    const Vector3 eye = center - dir * (radius + sunDistance);
    Matrix view = Matrix::CreateLookAt(eye, center, up);
    Matrix proj = Matrix::CreateOrthographic(radius * 2.0f, radius * 2.0f,
                                             0.0f, radius * 2.0f + sunDistance);

    if (shadowResolution > 0){
        Matrix vp = view * proj;
        Vector4 origin = Vector4::Transform(Vector4(0, 0, 0, 1), vp);
        const float halfRes = shadowResolution * 0.5f;
        origin *= halfRes;
        Vector4 rounded(roundf(origin.x), roundf(origin.y), origin.z, origin.w);
        Vector4 offset = rounded - origin;
        offset *= 1.0f / halfRes;
        proj._41 += offset.x;
        proj._42 += offset.y;
    }

    r.view = view;
    r.proj = proj;
    r.viewProj = view * proj;
    r.center = center;
    r.radius = radius;
    return r;
}

}
