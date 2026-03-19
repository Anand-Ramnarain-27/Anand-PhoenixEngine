#pragma once
#include <array>
#include <cstdint>
#include <d3dx12.h>
#include <DirectXMath.h>

namespace FaceProjection
{
    struct FaceDesc
    {
        DirectX::XMFLOAT3 front;
        DirectX::XMFLOAT3 up;
    };

    inline const std::array<FaceDesc, 6>& faces()
    {
        static const std::array<FaceDesc, 6> kFaces =
        { {
            { {  1,  0,  0 }, {  0,  1,  0 } },   // 0: +X
            { { -1,  0,  0 }, {  0,  1,  0 } },   // 1: -X
            { {  0,  1,  0 }, {  0,  0, -1 } },   // 2: +Y
            { {  0, -1,  0 }, {  0,  0,  1 } },   // 3: -Y
            { {  0,  0,  1 }, {  0,  1,  0 } },   // 4: +Z
            { {  0,  0, -1 }, {  0,  1,  0 } },   // 5: -Z
        } };
        return kFaces;
    }

    inline DirectX::XMMATRIX viewProj(uint32_t faceIndex) {
        using namespace DirectX;
        static const XMFLOAT3 fronts[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };
        static const XMFLOAT3 ups[6] = {
            {0, 1, 0}, {0, 1, 0},
            {0, 0, -1}, {0, 0,1},
            {0, 1, 0}, {0, 1, 0}
        };

        XMVECTOR eye = XMVectorZero();
        XMVECTOR at = XMLoadFloat3(&fronts[faceIndex]);
        XMVECTOR up = XMLoadFloat3(&ups[faceIndex]);
        XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 100.0f);
        return view * proj;
    }

    inline bool needsFlipZ(uint32_t) { return false; }
    inline bool needsFlipX(uint32_t) { return false; }
}