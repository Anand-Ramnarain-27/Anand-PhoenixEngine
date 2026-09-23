#include "Globals.h"
#include "ComponentTransform2D.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentTransform2D::computeLayout(const UIRect& parent){
    const Vector2 parentSize = parent.size();
    const Vector2 anchorBoxMin = parent.min + parentSize * anchorMin;
    const Vector2 anchorBoxMax = parent.min + parentSize * anchorMax;
    const Vector2 anchorSize = anchorBoxMax - anchorBoxMin;

    const Vector2 finalSize = (anchorSize + size) * scale;
    m_pivotPos = anchorBoxMin + anchorSize * pivot + position;

    m_rect.min = m_pivotPos - finalSize * pivot;
    m_rect.max = m_rect.min + finalSize;
}

namespace {
    struct AnchorPreset {
        const char* name;
        Vector2 anchorMin;
        Vector2 anchorMax;
    };

    const AnchorPreset kPresets[] = {
        { "Top Left",      { 0.f, 0.f }, { 0.f, 0.f } },
        { "Top",           { .5f, 0.f }, { .5f, 0.f } },
        { "Top Right",     { 1.f, 0.f }, { 1.f, 0.f } },
        { "Left",          { 0.f, .5f }, { 0.f, .5f } },
        { "Center",        { .5f, .5f }, { .5f, .5f } },
        { "Right",         { 1.f, .5f }, { 1.f, .5f } },
        { "Bottom Left",   { 0.f, 1.f }, { 0.f, 1.f } },
        { "Bottom",        { .5f, 1.f }, { .5f, 1.f } },
        { "Bottom Right",  { 1.f, 1.f }, { 1.f, 1.f } },
        { "Stretch (fill)", { 0.f, 0.f }, { 1.f, 1.f } },
    };
}

void ComponentTransform2D::onEditor(){
    ImGui::Checkbox("Visible##t2d", &visible);

    if (ImGui::BeginCombo("Anchor Preset##t2d", "Choose...")){
        for (const AnchorPreset& p : kPresets){
            if (!ImGui::Selectable(p.name)) continue;
            anchorMin = p.anchorMin;
            anchorMax = p.anchorMax;
            // Pivot follows the anchor point so `position` reads as the distance from that edge/corner.
            pivot = (p.anchorMin + p.anchorMax) * 0.5f;
            position = Vector2::Zero;
            if (p.anchorMin != p.anchorMax) size = Vector2::Zero;
        }
        ImGui::EndCombo();
    }

    ImGui::DragFloat2("Position##t2d", &position.x, 1.f);
    ImGui::DragFloat2("Size##t2d", &size.x, 1.f);
    ImGui::DragFloat2("Anchor Min##t2d", &anchorMin.x, 0.01f, 0.f, 1.f);
    ImGui::DragFloat2("Anchor Max##t2d", &anchorMax.x, 0.01f, 0.f, 1.f);
    ImGui::DragFloat2("Pivot##t2d", &pivot.x, 0.01f, 0.f, 1.f);
    ImGui::DragFloat("Rotation##t2d", &rotation, 0.5f, -360.f, 360.f);
    ImGui::DragFloat2("Scale##t2d", &scale.x, 0.01f);

    ImGui::Checkbox("Mask Children##t2d", &maskChildren);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Clips every descendant to this rect (scroll views, panels with overflowing content).\nIgnored while Rotation is non-zero.");
}

void ComponentTransform2D::onSave(std::string& outJson) const{
    UIJson::putFloats(outJson, "position", &position.x, 2);
    UIJson::putFloats(outJson, "size", &size.x, 2);
    UIJson::putFloats(outJson, "anchorMin", &anchorMin.x, 2);
    UIJson::putFloats(outJson, "anchorMax", &anchorMax.x, 2);
    UIJson::putFloats(outJson, "pivot", &pivot.x, 2);
    UIJson::putFloat(outJson, "rotation", rotation);
    UIJson::putFloats(outJson, "scale", &scale.x, 2);
    UIJson::putBool(outJson, "visible", visible);
    UIJson::putBool(outJson, "maskChildren", maskChildren);
}

void ComponentTransform2D::onLoad(const std::string& json){
    UIJson::Reader r(json);
    r.getFloats("position", &position.x, 2);
    r.getFloats("size", &size.x, 2);
    r.getFloats("anchorMin", &anchorMin.x, 2);
    r.getFloats("anchorMax", &anchorMax.x, 2);
    r.getFloats("pivot", &pivot.x, 2);
    r.getFloat("rotation", rotation);
    r.getFloats("scale", &scale.x, 2);
    r.getBool("visible", visible);
    r.getBool("maskChildren", maskChildren);
}
