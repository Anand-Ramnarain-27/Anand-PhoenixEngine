#include "Globals.h"
#include "CurveWidget.h"

namespace {
    constexpr float kGraphSize = 120.f;
    constexpr float kHandleRadius = 5.f;

    ImVec2 ToScreen(const ImVec2& origin, float size, float x, float y){
        return ImVec2(origin.x + x * size, origin.y + (1.f - y) * size);
    }

    bool DragHandle(const char* id, const ImVec2& origin, float size, float& x, float& y){
        ImVec2 pos = ToScreen(origin, size, x, y);
        ImGui::SetCursorScreenPos(ImVec2(pos.x - kHandleRadius, pos.y - kHandleRadius));
        ImGui::InvisibleButton(id, ImVec2(kHandleRadius * 2.f, kHandleRadius * 2.f));
        bool changed = false;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            x = std::clamp(x + delta.x / size, -0.5f, 1.5f);
            y = std::clamp(y - delta.y / size, -0.5f, 1.5f);
            changed = true;
        }
        return changed;
    }
}

bool CurveWidget::Edit(const char* label, EaseCurve& curve, float* initVal, float* endVal,
                       float dragSpeed, float minVal, float maxVal){
    ImGui::PushID(label);
    bool changed = false;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    const float size = kGraphSize;

    // Background + grid.
    draw->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32(20, 20, 24, 255));
    for (int i = 1; i < 4; ++i) {
        float t = (float)i / 4.f;
        draw->AddLine(ImVec2(origin.x + t * size, origin.y), ImVec2(origin.x + t * size, origin.y + size), IM_COL32(60, 60, 68, 255));
        draw->AddLine(ImVec2(origin.x, origin.y + t * size), ImVec2(origin.x + size, origin.y + t * size), IM_COL32(60, 60, 68, 255));
    }
    draw->AddRect(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32(90, 90, 100, 255));

    // Curve polyline.
    constexpr int kSamples = 32;
    ImVec2 prev = ToScreen(origin, size, 0.f, curve.Eval(0.f));
    for (int i = 1; i <= kSamples; ++i) {
        float t = (float)i / (float)kSamples;
        ImVec2 cur = ToScreen(origin, size, t, curve.Eval(t));
        draw->AddLine(prev, cur, IM_COL32(232, 96, 110, 255), 2.f);
        prev = cur;
    }

    // Tangent guide lines + control point handles.
    ImVec2 p0 = ToScreen(origin, size, 0.f, 0.f);
    ImVec2 p3 = ToScreen(origin, size, 1.f, 1.f);
    ImVec2 p1 = ToScreen(origin, size, curve.p1x, curve.p1y);
    ImVec2 p2 = ToScreen(origin, size, curve.p2x, curve.p2y);
    draw->AddLine(p0, p1, IM_COL32(120, 120, 130, 200), 1.f);
    draw->AddLine(p3, p2, IM_COL32(120, 120, 130, 200), 1.f);
    draw->AddCircleFilled(p1, kHandleRadius, IM_COL32(74, 168, 232, 255));
    draw->AddCircleFilled(p2, kHandleRadius, IM_COL32(232, 193, 74, 255));

    // Reserve layout space, then overlay invisible drag buttons on the handles.
    ImGui::Dummy(ImVec2(size, size));
    if (DragHandle("##p1", origin, size, curve.p1x, curve.p1y)) changed = true;
    if (DragHandle("##p2", origin, size, curve.p2x, curve.p2y)) changed = true;
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size));

    if (ImGui::Button("Linear"))   { curve.p1x = 0.f;   curve.p1y = 0.f; curve.p2x = 1.f;  curve.p2y = 1.f; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Ease In"))  { curve.p1x = 0.42f; curve.p1y = 0.f; curve.p2x = 1.f;  curve.p2y = 1.f; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Ease Out")) { curve.p1x = 0.f;   curve.p1y = 0.f; curve.p2x = 0.58f; curve.p2y = 1.f; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Ease In Out")) { curve.p1x = 0.42f; curve.p1y = 0.f; curve.p2x = 0.58f; curve.p2y = 1.f; changed = true; }

    if (initVal) changed |= ImGui::DragFloat("init", initVal, dragSpeed, minVal, maxVal);
    if (endVal)  changed |= ImGui::DragFloat("end", endVal, dragSpeed, minVal, maxVal);

    ImGui::PopID();
    return changed;
}
