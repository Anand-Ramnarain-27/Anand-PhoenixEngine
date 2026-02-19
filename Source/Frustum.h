#pragma once

// ?????????????????????????????????????????????????????????????????????????????
// Frustum.h
//
// CPU-side view frustum built from camera parameters (geometric approach from
// the lecture: 8 corners + 6 planes).
//
// Usage:
//   Frustum f = Frustum::fromCamera(pos, fwd, right, up, fovY, aspect, near, far);
//   bool visible = f.intersectsAABB(aabbMin, aabbMax);
//   bool visible = f.intersectsOBB(center, halfExtents, axes);
// ?????????????????????????????????????????????????????????????????????????????

#include <array>
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct FrustumPlane
{
    Vector3 normal;  
    float   d;       

    float signedDist(const Vector3& p) const { return normal.Dot(p) - d; }
};

struct Frustum
{
    enum PlaneIdx { Near = 0, Far, Left, Right, Top, Bottom, COUNT };
    std::array<FrustumPlane, COUNT> planes;

    enum CornerIdx
    {
        NTL = 0, NTR, NBL, NBR,   
        FTL, FTR, FBL, FBR,  
        CORNER_COUNT
    };
    std::array<Vector3, CORNER_COUNT> corners;
    bool cornersValid = false;

    static Frustum fromCamera(
        const Vector3& pos,
        const Vector3& fwd, 
        const Vector3& right,
        const Vector3& up,   
        float fovY,       
        float aspect,       
        float nearDist,
        float farDist)
    {
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

        f.buildPlane(PlaneIdx::Near, nc, fwd);
        f.buildPlane(PlaneIdx::Far, fc, -fwd);
        f.buildPlaneFromPoints(PlaneIdx::Left, pos, f.corners[NTL], f.corners[FTL]);
        f.buildPlaneFromPoints(PlaneIdx::Right, pos, f.corners[FTR], f.corners[NTR]);
        f.buildPlaneFromPoints(PlaneIdx::Top, pos, f.corners[NTR], f.corners[FTR]);
        f.buildPlaneFromPoints(PlaneIdx::Bottom, pos, f.corners[FBL], f.corners[NBL]);

        return f;
    }

    bool intersectsAABB(const Vector3& aabbMin, const Vector3& aabbMax) const
    {
        const Vector3 verts[8] =
        {
            { aabbMin.x, aabbMin.y, aabbMin.z },
            { aabbMax.x, aabbMin.y, aabbMin.z },
            { aabbMin.x, aabbMax.y, aabbMin.z },
            { aabbMax.x, aabbMax.y, aabbMin.z },
            { aabbMin.x, aabbMin.y, aabbMax.z },
            { aabbMax.x, aabbMin.y, aabbMax.z },
            { aabbMin.x, aabbMax.y, aabbMax.z },
            { aabbMax.x, aabbMax.y, aabbMax.z },
        };

        for (const FrustumPlane& plane : planes)
        {
            int outCount = 0;
            for (const Vector3& v : verts)
                if (plane.signedDist(v) < 0.0f) ++outCount;

            if (outCount == 8) return false; 
        }
        return true;
    }

    bool intersectsOBB(const Vector3& center,
        const Vector3& he,
        const Vector3 axes[3]) const
    {
        const Vector3 verts[8] =
        {
            center + axes[0] * he.x + axes[1] * he.y + axes[2] * he.z,
            center - axes[0] * he.x + axes[1] * he.y + axes[2] * he.z,
            center + axes[0] * he.x - axes[1] * he.y + axes[2] * he.z,
            center - axes[0] * he.x - axes[1] * he.y + axes[2] * he.z,
            center + axes[0] * he.x + axes[1] * he.y - axes[2] * he.z,
            center - axes[0] * he.x + axes[1] * he.y - axes[2] * he.z,
            center + axes[0] * he.x - axes[1] * he.y - axes[2] * he.z,
            center - axes[0] * he.x - axes[1] * he.y - axes[2] * he.z,
        };

        for (const FrustumPlane& plane : planes)
        {
            int outCount = 0;
            for (const Vector3& v : verts)
                if (plane.signedDist(v) < 0.0f) ++outCount;
            if (outCount == 8) return false;
        }
        return true;
    }

    bool containsPoint(const Vector3& p) const
    {
        for (const FrustumPlane& plane : planes)
            if (plane.signedDist(p) < 0.0f) return false;
        return true;
    }

private:
    void buildPlane(int idx, const Vector3& pointOnPlane, const Vector3& inwardNormal)
    {
        planes[idx].normal = inwardNormal;
        planes[idx].d = inwardNormal.Dot(pointOnPlane);
    }

    void buildPlaneFromPoints(int idx, const Vector3& A, const Vector3& B, const Vector3& C)
    {
        Vector3 ab = B - A;
        Vector3 ac = C - A;
        Vector3 n = ab.Cross(ac);
        n.Normalize();
        planes[idx].normal = n;
        planes[idx].d = n.Dot(A);
    }
};