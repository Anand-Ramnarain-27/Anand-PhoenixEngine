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
        const FaceDesc& fd = faces()[faceIndex];
        DirectX::XMVECTOR eye = DirectX::XMVectorZero();
        DirectX::XMVECTOR at = DirectX::XMLoadFloat3(&fd.front);
        DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&fd.up);
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtRH(eye, at, up);
        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovRH(DirectX::XM_PIDIV2, 1.0f, 0.1f, 100.0f);
        return view * proj;
    }

    inline bool needsFlipZ(uint32_t faceIndex) { return faceIndex == 0 || faceIndex == 1; }
    inline bool needsFlipX(uint32_t faceIndex) { return !needsFlipZ(faceIndex); }
}