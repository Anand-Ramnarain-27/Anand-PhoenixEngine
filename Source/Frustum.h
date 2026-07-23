#pragma once
#include <array>
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct FrustumPlane {
    Vector3 normal;
    float d;

    float signedDist(const Vector3& p) const { return normal.Dot(p) - d; }
};

struct Frustum {
    enum PlaneIdx { Near = 0, Far, Left, Right, Top, Bottom, COUNT };
    enum CornerIdx { NTL = 0, NTR, NBL, NBR, FTL, FTR, FBL, FBR, CORNER_COUNT };

    std::array<FrustumPlane, COUNT> planes;
    std::array<Vector3, CORNER_COUNT> corners;
    bool cornersValid = false;

    static Frustum fromCamera(const Vector3& pos, const Vector3& fwd, const Vector3& right, const Vector3& up, float fovY, float aspect, float nearDist, float farDist){
        Frustum f;
        const float hNear = tanf(fovY * 0.5f) * nearDist;
        const float wNear = hNear * aspect;
        const float hFar = tanf(fovY * 0.5f) * farDist;
        const float wFar = hFar * aspect;
        const Vector3 nc = pos + fwd * nearDist;
        const Vector3 fc = pos + fwd * farDist;

        f.corners[NTL] = nc + up * hNear - right * wNear;
        f.corners[NTR] = nc + up * hNear + right * wNear;
        f.corners[NBL] = nc - up * hNear - right * wNear;
        f.corners[NBR] = nc - up * hNear + right * wNear;
        f.corners[FTL] = fc + up * hFar - right * wFar;
        f.corners[FTR] = fc + up * hFar + right * wFar;
        f.corners[FBL] = fc - up * hFar - right * wFar;
        f.corners[FBR] = fc - up * hFar + right * wFar;
        f.cornersValid = true;

        f.buildPlane(Near, nc, fwd);
        f.buildPlane(Far, fc, -fwd);
        // Side planes: apex + two adjacent near corners (non-collinear), oriented inward.
        const Vector3 interior = pos + fwd * ((nearDist + farDist) * 0.5f);
        f.buildSidePlane(Left,   pos, f.corners[NTL], f.corners[NBL], interior);
        f.buildSidePlane(Right,  pos, f.corners[NTR], f.corners[NBR], interior);
        f.buildSidePlane(Top,    pos, f.corners[NTL], f.corners[NTR], interior);
        f.buildSidePlane(Bottom, pos, f.corners[NBL], f.corners[NBR], interior);
        return f;
    }

    static Frustum fromViewProj(const Matrix& viewProj){
        Matrix inv = viewProj.Invert();
        static const Vector3 ndc[CORNER_COUNT] = {
            {-1.f, +1.f, 0.f}, {+1.f, +1.f, 0.f}, {-1.f, -1.f, 0.f}, {+1.f, -1.f, 0.f},
            {-1.f, +1.f, 1.f}, {+1.f, +1.f, 1.f}, {-1.f, -1.f, 1.f}, {+1.f, -1.f, 1.f}
        };
        Frustum f;
        Vector3 centroid(0.f, 0.f, 0.f);
        for (int i = 0; i < CORNER_COUNT; ++i){
            Vector4 p = Vector4::Transform(Vector4(ndc[i].x, ndc[i].y, ndc[i].z, 1.f), inv);
            const float invw = (fabsf(p.w) > 1e-8f) ? 1.f / p.w : 1.f;
            f.corners[i] = Vector3(p.x * invw, p.y * invw, p.z * invw);
            centroid += f.corners[i];
        }
        centroid *= (1.f / float(CORNER_COUNT));
        f.cornersValid = true;
        f.buildSidePlane(Near,   f.corners[NTL], f.corners[NTR], f.corners[NBL], centroid);
        f.buildSidePlane(Far,    f.corners[FTL], f.corners[FTR], f.corners[FBL], centroid);
        f.buildSidePlane(Left,   f.corners[NTL], f.corners[NBL], f.corners[FTL], centroid);
        f.buildSidePlane(Right,  f.corners[NTR], f.corners[NBR], f.corners[FTR], centroid);
        f.buildSidePlane(Top,    f.corners[NTL], f.corners[NTR], f.corners[FTL], centroid);
        f.buildSidePlane(Bottom, f.corners[NBL], f.corners[NBR], f.corners[FBL], centroid);
        return f;
    }

    bool testVertsAgainstPlanes(const Vector3 verts[8]) const{
        for (const FrustumPlane& plane : planes){
            int outCount = 0;
            for (int i = 0; i < 8; ++i) if (plane.signedDist(verts[i]) < 0.0f) ++outCount;
            if (outCount == 8) return false;
        }
        return true;
    }

    bool intersectsAABB(const Vector3& mn, const Vector3& mx) const{
        const Vector3 verts[8] = {
            {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
            {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}
        };
        return testVertsAgainstPlanes(verts);
    }

    bool intersectsOBB(const Vector3& center, const Vector3& he, const Vector3 axes[3]) const{
        const Vector3 verts[8] = {
            center + axes[0] * he.x + axes[1] * he.y + axes[2] * he.z,
            center - axes[0] * he.x + axes[1] * he.y + axes[2] * he.z,
            center + axes[0] * he.x - axes[1] * he.y + axes[2] * he.z,
            center - axes[0] * he.x - axes[1] * he.y + axes[2] * he.z,
            center + axes[0] * he.x + axes[1] * he.y - axes[2] * he.z,
            center - axes[0] * he.x + axes[1] * he.y - axes[2] * he.z,
            center + axes[0] * he.x - axes[1] * he.y - axes[2] * he.z,
            center - axes[0] * he.x - axes[1] * he.y - axes[2] * he.z
        };
        return testVertsAgainstPlanes(verts);
    }

    bool containsPoint(const Vector3& p) const{
        for (const FrustumPlane& plane : planes) if (plane.signedDist(p) < 0.0f) return false;
        return true;
    }

private:
    void buildPlane(int idx, const Vector3& pointOnPlane, const Vector3& inwardNormal){
        planes[idx].normal = inwardNormal;
        planes[idx].d = inwardNormal.Dot(pointOnPlane);
    }

    void buildSidePlane(int idx, const Vector3& A, const Vector3& B, const Vector3& C, const Vector3& interior){
        Vector3 n = (B - A).Cross(C - A);
        n.Normalize();
        float d = n.Dot(A);
        if (n.Dot(interior) - d < 0.0f){ n = -n; d = -d; } // ensure normal points into the frustum
        planes[idx].normal = n;
        planes[idx].d = d;
    }
};
