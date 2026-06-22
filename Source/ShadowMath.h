#pragma once
#include <SimpleMath.h>
#include <cstdint>

using namespace DirectX::SimpleMath;

namespace ShadowMath {

    static constexpr int kMaxCascades = 4;

    struct DirShadowResult {
        Matrix view;
        Matrix proj;
        Matrix viewProj;
        Vector3 center;
        float radius;
    };

    DirShadowResult DirectionalLightViewProj(const Matrix& camView,
                                             const Matrix& camProj,
                                             const Vector3& lightDir,
                                             float nearDist, float farDist,
                                             float sunDistance,
                                             uint32_t shadowResolution);

    void ExtractNearFar(const Matrix& proj, float& outNear, float& outFar);

    Matrix SpotLightViewProj(const Vector3& pos, const Vector3& dir,
                             float outerAngleRad, float range);

    void PointLightFaceViewProj(const Vector3& pos, float nearDist, float range,
                                Matrix out[6]);

    void CascadeSplits(float nearDist, float farDist, int count, float lambda,
                       float* outSplits);
}
