#include "Globals.h"
#include "ComponentLabel.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentLabel::onEditor(){
    ImGui::Checkbox("Enabled##label", &enabled);

    char buf[1024];
    strncpy_s(buf, text.c_str(), _TRUNCATE);
    if (ImGui::InputTextMultiline("Text##label", buf, sizeof(buf), ImVec2(0.f, 60.f)))
        text = buf;

    char fontBuf[128];
    strncpy_s(fontBuf, fontName.c_str(), _TRUNCATE);
    if (ImGui::InputText("Font##label", fontBuf, sizeof(fontBuf)))
        fontName = fontBuf;

    ImGui::DragFloat("Font Size##label", &fontSize, 0.5f, 1.f, 512.f);
    ImGui::ColorEdit4("Color##label", &color.x);

    static const char* kH[] = { "Left", "Center", "Right" };
    static const char* kV[] = { "Top", "Middle", "Bottom" };
    int h = (int)hAlign, v = (int)vAlign;
    if (ImGui::Combo("Horizontal##label", &h, kH, IM_ARRAYSIZE(kH))) hAlign = (HAlign)h;
    if (ImGui::Combo("Vertical##label", &v, kV, IM_ARRAYSIZE(kV))) vAlign = (VAlign)v;
}

void ComponentLabel::onSave(std::string& outJson) const{
    UIJson::putBool(outJson, "enabled", enabled);
    UIJson::putString(outJson, "text", text);
    UIJson::putString(outJson, "fontName", fontName);
    UIJson::putFloat(outJson, "fontSize", fontSize);
    UIJson::putFloats(outJson, "color", &color.x, 4);
    UIJson::putInt(outJson, "hAlign", (int)hAlign);
    UIJson::putInt(outJson, "vAlign", (int)vAlign);
}

void ComponentLabel::onLoad(const std::string& json){
    UIJson::Reader r(json);
    r.getBool("enabled", enabled);
    r.getString("text", text);
    r.getString("fontName", fontName);
    r.getFloat("fontSize", fontSize);
    r.getFloats("color", &color.x, 4);
    int h = (int)hAlign, v = (int)vAlign;
    if (r.getInt("hAlign", h)) hAlign = (HAlign)h;
    if (r.getInt("vAlign", v)) vAlign = (VAlign)v;
}
