#include "Globals.h"
#include "ComponentSlider.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentSlider::onEditor(){
    ImGui::Checkbox("Interactable##slider", &interactable);
    ImGui::Checkbox("Navigable (Tab)##slider", &navigable);

    ImGui::DragFloat("Min##slider", &minValue, 0.1f);
    ImGui::DragFloat("Max##slider", &maxValue, 0.1f);
    ImGui::Checkbox("Whole Numbers##slider", &wholeNumbers);
    float v = value;
    if (ImGui::SliderFloat("Value##slider", &v, lowest(), highest() > lowest() ? highest() : lowest() + 1.f))
        setValue(v);

    static const char* kDirections[] = { "Left To Right", "Right To Left", "Bottom To Top", "Top To Bottom" };
    int dir = (int)direction;
    if (ImGui::Combo("Direction##slider", &dir, kDirections, IM_ARRAYSIZE(kDirections)))
        direction = (Direction)dir;

    ImGui::DragFloat("Track Thickness##slider", &trackThickness, 0.25f, 1.f, 256.f);
    ImGui::DragFloat2("Handle Size##slider", &handleSize.x, 0.5f, 1.f, 256.f);
    ImGui::ColorEdit4("Track##slider", &trackColor.x);
    ImGui::ColorEdit4("Fill##slider", &fillColor.x);
    ImGui::ColorEdit4("Handle##slider", &handleColor.x);
}

void ComponentSlider::onSave(std::string& outJson) const{
    saveSelectable(outJson);
    UIJson::putFloat(outJson, "value", value);
    UIJson::putFloat(outJson, "minValue", minValue);
    UIJson::putFloat(outJson, "maxValue", maxValue);
    UIJson::putBool(outJson, "wholeNumbers", wholeNumbers);
    UIJson::putInt(outJson, "direction", (int)direction);
    UIJson::putFloat(outJson, "trackThickness", trackThickness);
    UIJson::putFloats(outJson, "handleSize", &handleSize.x, 2);
    UIJson::putFloats(outJson, "trackColor", &trackColor.x, 4);
    UIJson::putFloats(outJson, "fillColor", &fillColor.x, 4);
    UIJson::putFloats(outJson, "handleColor", &handleColor.x, 4);
}

void ComponentSlider::onLoad(const std::string& json){
    UIJson::Reader r(json);
    loadSelectable(r);
    r.getFloat("value", value);
    r.getFloat("minValue", minValue);
    r.getFloat("maxValue", maxValue);
    r.getBool("wholeNumbers", wholeNumbers);
    int dir = (int)direction;
    if (r.getInt("direction", dir)) direction = (Direction)dir;
    r.getFloat("trackThickness", trackThickness);
    r.getFloats("handleSize", &handleSize.x, 2);
    r.getFloats("trackColor", &trackColor.x, 4);
    r.getFloats("fillColor", &fillColor.x, 4);
    r.getFloats("handleColor", &handleColor.x, 4);
}
