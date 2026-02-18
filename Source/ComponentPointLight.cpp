#include "Globals.h"
#include "ComponentPointLight.h"
#include <imgui.h>

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

ComponentPointLight::ComponentPointLight(GameObject* owner) : Component(owner) {}

void ComponentPointLight::onEditor()
{
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::ColorEdit3("Color", &color.x);
    ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.f, 10.f);
    ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.f);
}

void ComponentPointLight::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value col(kArrayType);
    col.PushBack(color.x, a).PushBack(color.y, a).PushBack(color.z, a);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("radius", radius, a);
    doc.AddMember("enabled", enabled, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentPointLight::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("color")) { auto& c = doc["color"]; color = { c[0].GetFloat(),c[1].GetFloat(),c[2].GetFloat() }; }
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("radius"))    radius = doc["radius"].GetFloat();
    if (doc.HasMember("enabled"))   enabled = doc["enabled"].GetBool();
}