#pragma once
#include <imgui.h>
#include <algorithm>

// A simple cubic-bezier easing curve, running from (0,0) to (1,1).
// p1/p2 are the two interior control points (CSS cubic-bezier convention).
struct EaseCurve {
    // Defaults to a linear curve (p1=(0,0), p2=(1,1)) so existing lerp-based
    // over-lifetime values keep their behaviour until a curve is edited.
    float p1x = 0.f, p1y = 0.f;
    float p2x = 1.f, p2y = 1.f;

    // Solves x(u) = t for u via bisection, then returns y(u) — i.e. the eased
    // value for normalised input t in [0,1].
    float Eval(float t) const{
        t = std::clamp(t, 0.f, 1.f);
        float lo = 0.f, hi = 1.f, u = t;
        for (int i = 0; i < 20; ++i) {
            u = (lo + hi) * 0.5f;
            float x = bez(p1x, p2x, u);
            if (x < t) lo = u; else hi = u;
        }
        return bez(p1y, p2y, u);
    }

private:
    static float bez(float a, float b, float u){
        float mu = 1.f - u;
        return 3.f * mu * mu * u * a + 3.f * mu * u * u * b + u * u * u;
    }
};

namespace CurveWidget {
    // Draws a small interactive curve graph (draggable control points) with
    // Linear / Ease In / Ease Out / Ease In-Out preset buttons, plus "init"/"end"
    // value drag floats below it. Returns true if curve or values changed.
    bool Edit(const char* label, EaseCurve& curve, float* initVal, float* endVal,
              float dragSpeed = 0.01f, float minVal = 0.f, float maxVal = 100.f);
}
