#pragma once
// Finite / range checks for VFX data, with once-per-object rejection logging.

#include "Globals.h"
#include <cmath>

class GameObject;

/// Sanity checks for effect data (decals, lights, VFX meshes). A bad value - a NaN from a script, a zero-size
/// decal box - is refused or clamped, never sent to the GPU. Each object logs once per kind of rejection.
/// Defined in API/Phoenix_VFX.cpp (built into the engine, the player and PhoenixCore.lib).
namespace VfxGuards {
    constexpr float kMinDecalScale = 0.01f;

    inline bool finite(float v){ return std::isfinite(v); }
    inline bool finite(const Vector2& v){ return finite(v.x) && finite(v.y); }
    inline bool finite(const Vector3& v){ return finite(v.x) && finite(v.y) && finite(v.z); }
    inline bool finite(const Vector4& v){ return finite(v.x) && finite(v.y) && finite(v.z) && finite(v.w); }
    inline bool finite(const Quaternion& q){ return finite(q.x) && finite(q.y) && finite(q.z) && finite(q.w); }
    inline bool finite(const Matrix& m){
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (!finite(m.m[r][c])) return false;
        return true;
    }

    /// Logs "[VFX] rejected <what> on <object>: <detail>" the first time (object, what) is seen.
    void RejectOnce(const GameObject* go, const char* what, const char* detailFmt, ...);

    /// A decal's world matrix: non-finite -> false (skip it); any axis shorter than kMinDecalScale is stretched to
    /// it (a zero-length axis would make the inverse blow up and the box cover the screen).
    bool SanitizeDecalWorld(const GameObject* go, Matrix& world);
}
