#pragma once
#include <imgui.h>
#include <algorithm>

struct EaseCurve {
    float p1x = 0.f, p1y = 0.f;
    float p2x = 1.f, p2y = 1.f;

    float Eval(float t) const{
        t = std::clamp(t, 0.f, 1.f);
        float lo = 0.f, hi = 1.f, u = t;
        for (int i = 0; i < 20; ++i){
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
    bool Edit(const char* label, EaseCurve& curve, float* initVal, float* endVal,
              float dragSpeed = 0.01f, float minVal = 0.f, float maxVal = 100.f);
}
