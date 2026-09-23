#include "Globals.h"
#include "ComponentInputBox.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentInputBox::onEditor(){
    ImGui::Checkbox("Interactable##input", &interactable);
    ImGui::Checkbox("Navigable (Tab)##input", &navigable);

    char buf[512];
    strncpy_s(buf, text.c_str(), _TRUNCATE);
    if (ImGui::InputText("Text##input", buf, sizeof(buf)))
        setText(buf);

    char hint[128];
    strncpy_s(hint, placeholder.c_str(), _TRUNCATE);
    if (ImGui::InputText("Placeholder##input", hint, sizeof(hint)))
        placeholder = hint;

    ImGui::DragInt("Max Length##input", &maxLength, 1.f, 0, 512);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = unlimited");

    static const char* kTypes[] = { "Standard", "Integer", "Decimal", "Alphanumeric" };
    int type = (int)contentType;
    if (ImGui::Combo("Content##input", &type, kTypes, IM_ARRAYSIZE(kTypes)))
        contentType = (ContentType)type;
    ImGui::Checkbox("Password##input", &password);

    char fontBuf[128];
    strncpy_s(fontBuf, fontName.c_str(), _TRUNCATE);
    if (ImGui::InputText("Font##input", fontBuf, sizeof(fontBuf)))
        fontName = fontBuf;
    ImGui::DragFloat("Font Size##input", &fontSize, 0.5f, 1.f, 256.f);
    ImGui::DragFloat2("Padding##input", &padding.x, 0.5f, 0.f, 128.f);

    ImGui::ColorEdit4("Background##input", &backgroundColor.x);
    ImGui::ColorEdit4("Focus Outline##input", &focusColor.x);
    ImGui::ColorEdit4("Text##input", &textColor.x);
    ImGui::ColorEdit4("Placeholder##inputColor", &placeholderColor.x);
    ImGui::ColorEdit4("Caret##input", &caretColor.x);
    ImGui::ColorEdit4("Selection##input", &selectionColor.x);
}

void ComponentInputBox::onSave(std::string& outJson) const{
    saveSelectable(outJson);
    UIJson::putString(outJson, "text", text);
    UIJson::putString(outJson, "placeholder", placeholder);
    UIJson::putInt(outJson, "maxLength", maxLength);
    UIJson::putInt(outJson, "contentType", (int)contentType);
    UIJson::putBool(outJson, "password", password);
    UIJson::putString(outJson, "fontName", fontName);
    UIJson::putFloat(outJson, "fontSize", fontSize);
    UIJson::putFloats(outJson, "padding", &padding.x, 2);
    UIJson::putFloats(outJson, "backgroundColor", &backgroundColor.x, 4);
    UIJson::putFloats(outJson, "focusColor", &focusColor.x, 4);
    UIJson::putFloats(outJson, "textColor", &textColor.x, 4);
    UIJson::putFloats(outJson, "placeholderColor", &placeholderColor.x, 4);
    UIJson::putFloats(outJson, "caretColor", &caretColor.x, 4);
    UIJson::putFloats(outJson, "selectionColor", &selectionColor.x, 4);
}

void ComponentInputBox::onLoad(const std::string& json){
    UIJson::Reader r(json);
    loadSelectable(r);
    r.getInt("maxLength", maxLength);
    int type = (int)contentType;
    if (r.getInt("contentType", type)) contentType = (ContentType)type;
    r.getBool("password", password);
    r.getString("placeholder", placeholder);
    r.getString("fontName", fontName);
    r.getFloat("fontSize", fontSize);
    r.getFloats("padding", &padding.x, 2);
    r.getFloats("backgroundColor", &backgroundColor.x, 4);
    r.getFloats("focusColor", &focusColor.x, 4);
    r.getFloats("textColor", &textColor.x, 4);
    r.getFloats("placeholderColor", &placeholderColor.x, 4);
    r.getFloats("caretColor", &caretColor.x, 4);
    r.getFloats("selectionColor", &selectionColor.x, 4);

    // Applied after the limits and content type above are known, so a saved value is filtered like typed input.
    std::string saved;
    if (r.getString("text", saved)) setText(saved);
}
