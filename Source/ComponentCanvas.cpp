#include "Globals.h"
#include "ComponentCanvas.h"
#include "UIJson.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

float ComponentCanvas::getScaleFactor(float screenW, float screenH) const{
    if (scaleMode == ScaleMode::ConstantPixelSize) return 1.f;

    const float refW = std::max(1.f, referenceResolution.x);
    const float refH = std::max(1.f, referenceResolution.y);
    const float match = std::clamp(matchWidthOrHeight, 0.f, 1.f);
    // Blend the width/height ratios logarithmically so 0.5 keeps the layout balanced on any aspect ratio.
    return std::pow(std::max(screenW, 1.f) / refW, 1.f - match) * std::pow(std::max(screenH, 1.f) / refH, match);
}

void ComponentCanvas::onEditor(){
    ImGui::Checkbox("Enabled##canvas", &enabled);
    ImGui::DragInt("Sort Order##canvas", &sortOrder, 1.f);

    static const char* kModes[] = { "Constant Pixel Size", "Scale With Screen Size" };
    int mode = (int)scaleMode;
    if (ImGui::Combo("Scale Mode##canvas", &mode, kModes, IM_ARRAYSIZE(kModes)))
        scaleMode = (ScaleMode)mode;

    if (scaleMode == ScaleMode::ScaleWithScreenSize){
        ImGui::DragFloat2("Reference Resolution##canvas", &referenceResolution.x, 1.f, 1.f, 16384.f);
        ImGui::SliderFloat("Match Width/Height##canvas", &matchWidthOrHeight, 0.f, 1.f);
    }
}

void ComponentCanvas::onSave(std::string& outJson) const{
    UIJson::putBool(outJson, "enabled", enabled);
    UIJson::putInt(outJson, "sortOrder", sortOrder);
    UIJson::putInt(outJson, "scaleMode", (int)scaleMode);
    UIJson::putFloats(outJson, "referenceResolution", &referenceResolution.x, 2);
    UIJson::putFloat(outJson, "matchWidthOrHeight", matchWidthOrHeight);
}

void ComponentCanvas::onLoad(const std::string& json){
    UIJson::Reader r(json);
    r.getBool("enabled", enabled);
    r.getInt("sortOrder", sortOrder);
    int mode = (int)scaleMode;
    if (r.getInt("scaleMode", mode)) scaleMode = (ScaleMode)mode;
    r.getFloats("referenceResolution", &referenceResolution.x, 2);
    r.getFloat("matchWidthOrHeight", matchWidthOrHeight);
}
