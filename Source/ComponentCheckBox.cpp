#include "Globals.h"
#include "ComponentCheckBox.h"
#include "AssetPickerWidget.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentCheckBox::onEditor(){
    ImGui::Checkbox("Interactable##checkbox", &interactable);
    ImGui::Checkbox("Navigable (Tab)##checkbox", &navigable);
    ImGui::Checkbox("Checked##checkbox", &checked);

    ImGui::DragFloat("Box Size##checkbox", &boxSize, 0.5f, 0.f, 512.f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Side of the box in canvas units. 0 uses the row height.");
    ImGui::ColorEdit4("Box##checkbox", &boxColor.x);
    ImGui::ColorEdit4("Check Mark##checkbox", &checkColor.x);
    ImGui::TextUnformatted("Check texture (optional)");
    AssetPicker::Draw("##checkboxMarkTex", checkTexture, AssetPicker::kTextures);
}

void ComponentCheckBox::onSave(std::string& outJson) const{
    saveSelectable(outJson);
    UIJson::putBool(outJson, "checked", checked);
    UIJson::putFloat(outJson, "boxSize", boxSize);
    UIJson::putFloats(outJson, "boxColor", &boxColor.x, 4);
    UIJson::putFloats(outJson, "checkColor", &checkColor.x, 4);
    UIJson::putString(outJson, "checkTexture", checkTexture);
}

void ComponentCheckBox::onLoad(const std::string& json){
    UIJson::Reader r(json);
    loadSelectable(r);
    r.getBool("checked", checked);
    r.getFloat("boxSize", boxSize);
    r.getFloats("boxColor", &boxColor.x, 4);
    r.getFloats("checkColor", &checkColor.x, 4);
    r.getString("checkTexture", checkTexture);
}
