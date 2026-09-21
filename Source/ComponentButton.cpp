#include "Globals.h"
#include "ComponentButton.h"
#include "AssetPickerWidget.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentButton::onEditor(){
    ImGui::Checkbox("Interactable##button", &interactable);
    ImGui::Checkbox("Navigable (Tab)##button", &navigable);

    ImGui::SeparatorText("Colour tint");
    ImGui::ColorEdit4("Normal##button", &normalColor.x);
    ImGui::ColorEdit4("Hover##button", &hoverColor.x);
    ImGui::ColorEdit4("Pressed##button", &pressedColor.x);
    ImGui::ColorEdit4("Disabled##button", &disabledColor.x);

    ImGui::SeparatorText("Sprite swap (optional)");
    ImGui::TextUnformatted("Hover");    AssetPicker::Draw("##buttonHoverTex", hoverTexture, AssetPicker::kTextures);
    ImGui::TextUnformatted("Pressed");  AssetPicker::Draw("##buttonPressedTex", pressedTexture, AssetPicker::kTextures);
    ImGui::TextUnformatted("Disabled"); AssetPicker::Draw("##buttonDisabledTex", disabledTexture, AssetPicker::kTextures);

    static const char* kStateNames[] = { "Normal", "Hovered", "Pressed", "Disabled" };
    ImGui::SeparatorText("Runtime");
    ImGui::Text("State: %s", kStateNames[(int)state]);
}

void ComponentButton::onSave(std::string& outJson) const{
    saveSelectable(outJson);
    UIJson::putFloats(outJson, "normalColor", &normalColor.x, 4);
    UIJson::putFloats(outJson, "hoverColor", &hoverColor.x, 4);
    UIJson::putFloats(outJson, "pressedColor", &pressedColor.x, 4);
    UIJson::putFloats(outJson, "disabledColor", &disabledColor.x, 4);
    UIJson::putString(outJson, "hoverTexture", hoverTexture);
    UIJson::putString(outJson, "pressedTexture", pressedTexture);
    UIJson::putString(outJson, "disabledTexture", disabledTexture);
}

void ComponentButton::onLoad(const std::string& json){
    UIJson::Reader r(json);
    loadSelectable(r);
    r.getFloats("normalColor", &normalColor.x, 4);
    r.getFloats("hoverColor", &hoverColor.x, 4);
    r.getFloats("pressedColor", &pressedColor.x, 4);
    r.getFloats("disabledColor", &disabledColor.x, 4);
    r.getString("hoverTexture", hoverTexture);
    r.getString("pressedTexture", pressedTexture);
    r.getString("disabledTexture", disabledTexture);
}
