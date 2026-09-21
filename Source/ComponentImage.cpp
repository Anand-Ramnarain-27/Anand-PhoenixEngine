#include "Globals.h"
#include "ComponentImage.h"
#include "AssetPickerWidget.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentImage::onEditor(){
    ImGui::Checkbox("Enabled##image", &enabled);
    ImGui::Checkbox("Raycast Target##image", &raycastTarget);
    AssetPicker::Draw("##uiImageTexture", texturePath, AssetPicker::kTextures);
    ImGui::ColorEdit4("Tint##image", &tint.x);

    ImGui::Checkbox("Use Source Rect##image", &useSourceRect);
    if (useSourceRect)
        ImGui::DragFloat4("Source Rect (x y w h)##image", &sourceRect.x, 1.f, 0.f, 16384.f);
}

void ComponentImage::onSave(std::string& outJson) const{
    UIJson::putBool(outJson, "enabled", enabled);
    UIJson::putBool(outJson, "raycastTarget", raycastTarget);
    UIJson::putString(outJson, "texturePath", texturePath);
    UIJson::putFloats(outJson, "tint", &tint.x, 4);
    UIJson::putBool(outJson, "useSourceRect", useSourceRect);
    UIJson::putFloats(outJson, "sourceRect", &sourceRect.x, 4);
}

void ComponentImage::onLoad(const std::string& json){
    UIJson::Reader r(json);
    r.getBool("enabled", enabled);
    r.getBool("raycastTarget", raycastTarget);
    r.getString("texturePath", texturePath);
    r.getFloats("tint", &tint.x, 4);
    r.getBool("useSourceRect", useSourceRect);
    r.getFloats("sourceRect", &sourceRect.x, 4);
}
