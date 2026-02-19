#pragma once

#include "Frustum.h"
#include <vector>

struct DebugLine
{
    Vector3 from;
    Vector3 to;
    Vector3 color;
};

class FrustumDebugDraw
{
public:
    std::vector<DebugLine> lines;

    void clear() { lines.clear(); }

    void addLine(const Vector3& from, const Vector3& to, const Vector3& color)
    {
        lines.push_back({ from, to, color });
    }

    void addAxes(const Vector3& origin,
        const Vector3& fwd,
        const Vector3& right,
        const Vector3& up,
        float scale = 1.0f)
    {
        addLine(origin, origin + right * scale, Vector3(1, 0, 0));
        addLine(origin, origin + up * scale, Vector3(0, 1, 0));
        addLine(origin, origin + fwd * scale, Vector3(0, 0, 1));
    }

    void addFrustum(const Frustum& f, const Vector3& color)
    {
        if (!f.cornersValid) return;

        const auto& c = f.corners;
        using CI = Frustum::CornerIdx;

        addLine(c[CI::NTL], c[CI::NTR], color);
        addLine(c[CI::NTR], c[CI::NBR], color);
        addLine(c[CI::NBR], c[CI::NBL], color);
        addLine(c[CI::NBL], c[CI::NTL], color);

        addLine(c[CI::FTL], c[CI::FTR], color);
        addLine(c[CI::FTR], c[CI::FBR], color);
        addLine(c[CI::FBR], c[CI::FBL], color);
        addLine(c[CI::FBL], c[CI::FTL], color);

        addLine(c[CI::NTL], c[CI::FTL], color);
        addLine(c[CI::NTR], c[CI::FTR], color);
        addLine(c[CI::NBL], c[CI::FBL], color);
        addLine(c[CI::NBR], c[CI::FBR], color);
    }

    void addAABB(const Vector3& mn, const Vector3& mx, const Vector3& color)
    {
        addLine({ mn.x, mn.y, mn.z }, { mx.x, mn.y, mn.z }, color);
        addLine({ mx.x, mn.y, mn.z }, { mx.x, mn.y, mx.z }, color);
        addLine({ mx.x, mn.y, mx.z }, { mn.x, mn.y, mx.z }, color);
        addLine({ mn.x, mn.y, mx.z }, { mn.x, mn.y, mn.z }, color);
        addLine({ mn.x, mx.y, mn.z }, { mx.x, mx.y, mn.z }, color);
        addLine({ mx.x, mx.y, mn.z }, { mx.x, mx.y, mx.z }, color);
        addLine({ mx.x, mx.y, mx.z }, { mn.x, mx.y, mx.z }, color);
        addLine({ mn.x, mx.y, mx.z }, { mn.x, mx.y, mn.z }, color);
        addLine({ mn.x, mn.y, mn.z }, { mn.x, mx.y, mn.z }, color);
        addLine({ mx.x, mn.y, mn.z }, { mx.x, mx.y, mn.z }, color);
        addLine({ mx.x, mn.y, mx.z }, { mx.x, mx.y, mx.z }, color);
        addLine({ mn.x, mn.y, mx.z }, { mn.x, mx.y, mx.z }, color);
    }

    size_t lineCount()   const { return lines.size(); }
    bool   empty()       const { return lines.empty(); }
};