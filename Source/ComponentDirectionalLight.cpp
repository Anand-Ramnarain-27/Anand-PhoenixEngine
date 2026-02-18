#include "Globals.h"
#include "ComponentDirectionalLight.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

ComponentDirectionalLight::ComponentDirectionalLight(GameObject* owner)
    : Component(owner) {
}

void ComponentDirectionalLight::onEditor()
{
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f);
    ImGui::ColorEdit3("Color", &color.x);
    ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.f, 10.f);
}

void ComponentDirectionalLight::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject();
    auto& a = doc.GetAllocator();
    Value dir(kArrayType);
    dir.PushBack(direction.x, a).PushBack(direction.y, a).PushBack(direction.z, a);
    Value col(kArrayType);
    col.PushBack(color.x, a).PushBack(color.y, a).PushBack(color.z, a);
    doc.AddMember("direction", dir, a);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("enabled", enabled, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentDirectionalLight::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("direction")) { auto& d = doc["direction"]; direction = { d[0].GetFloat(),d[1].GetFloat(),d[2].GetFloat() }; }
    if (doc.HasMember("color")) { auto& c = doc["color"];     color = { c[0].GetFloat(),c[1].GetFloat(),c[2].GetFloat() }; }
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("enabled"))   enabled = doc["enabled"].GetBool();
}