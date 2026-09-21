#include "Globals.h"
#include "ComponentProgressBar.h"
#include "AssetPickerWidget.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentProgressBar::onEditor(){
    ImGui::Checkbox("Enabled##progress", &enabled);

    ImGui::DragFloat("Min##progress", &minValue, 0.1f);
    ImGui::DragFloat("Max##progress", &maxValue, 0.1f);
    ImGui::SliderFloat("Value##progress", &value, minValue, maxValue > minValue ? maxValue : minValue + 1.f);
    ImGui::Text("Filled: %.0f%%", getNormalized() * 100.f);

    static const char* kDirections[] = { "Left To Right", "Right To Left", "Bottom To Top", "Top To Bottom" };
    int dir = (int)direction;
    if (ImGui::Combo("Direction##progress", &dir, kDirections, IM_ARRAYSIZE(kDirections)))
        direction = (FillDirection)dir;

    ImGui::ColorEdit4("Background##progress", &backgroundColor.x);
    ImGui::ColorEdit4("Fill##progress", &fillColor.x);

    ImGui::TextUnformatted("Background texture");
    AssetPicker::Draw("##progressBgTex", backgroundTexture, AssetPicker::kTextures);
    ImGui::TextUnformatted("Fill texture");
    AssetPicker::Draw("##progressFillTex", fillTexture, AssetPicker::kTextures);
}

void ComponentProgressBar::onSave(std::string& outJson) const{
    UIJson::putBool(outJson, "enabled", enabled);
    UIJson::putFloat(outJson, "value", value);
    UIJson::putFloat(outJson, "minValue", minValue);
    UIJson::putFloat(outJson, "maxValue", maxValue);
    UIJson::putInt(outJson, "direction", (int)direction);
    UIJson::putFloats(outJson, "backgroundColor", &backgroundColor.x, 4);
    UIJson::putFloats(outJson, "fillColor", &fillColor.x, 4);
    UIJson::putString(outJson, "backgroundTexture", backgroundTexture);
    UIJson::putString(outJson, "fillTexture", fillTexture);
}

void ComponentProgressBar::onLoad(const std::string& json){
    UIJson::Reader r(json);
    r.getBool("enabled", enabled);
    r.getFloat("value", value);
    r.getFloat("minValue", minValue);
    r.getFloat("maxValue", maxValue);
    int dir = (int)direction;
    if (r.getInt("direction", dir)) direction = (FillDirection)dir;
    r.getFloats("backgroundColor", &backgroundColor.x, 4);
    r.getFloats("fillColor", &fillColor.x, 4);
    r.getString("backgroundTexture", backgroundTexture);
    r.getString("fillTexture", fillTexture);
}
