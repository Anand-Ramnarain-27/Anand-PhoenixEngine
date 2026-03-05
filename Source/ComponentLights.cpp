#include "Globals.h"
#include "ComponentLights.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

namespace {

    void serializeVec3(Document& doc, Value& arr, const Vector3& v) {
        auto& a = doc.GetAllocator();
        arr.SetArray();
        arr.PushBack(v.x, a).PushBack(v.y, a).PushBack(v.z, a);
    }

    Vector3 deserializeVec3(const Value& arr) {
        return { arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat() };
    }

    void editorBaseLight(bool& enabled, Vector3& color, float& intensity) {
        ImGui::Checkbox("Enabled", &enabled);
        ImGui::ColorEdit3("Color", &color.x);
        ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.f, 10.f);
    }

}

ComponentDirectionalLight::ComponentDirectionalLight(GameObject* owner) : Component(owner) {}

void ComponentDirectionalLight::onEditor() {
    editorBaseLight(enabled, color, intensity);
    ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f);
}

void ComponentDirectionalLight::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value dir, col;
    serializeVec3(doc, dir, direction);
    serializeVec3(doc, col, color);
    doc.AddMember("direction", dir, a);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("enabled", enabled, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentDirectionalLight::onLoad(const std::string& jsonStr) {
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("direction")) direction = deserializeVec3(doc["direction"]);
    if (doc.HasMember("color")) color = deserializeVec3(doc["color"]);
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("enabled")) enabled = doc["enabled"].GetBool();
}

ComponentPointLight::ComponentPointLight(GameObject* owner) : Component(owner) {}

void ComponentPointLight::onEditor() {
    editorBaseLight(enabled, color, intensity);
    ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.f);
}

void ComponentPointLight::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value col;
    serializeVec3(doc, col, color);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("radius", radius, a);
    doc.AddMember("enabled", enabled, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentPointLight::onLoad(const std::string& jsonStr) {
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("color")) color = deserializeVec3(doc["color"]);
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("radius")) radius = doc["radius"].GetFloat();
    if (doc.HasMember("enabled")) enabled = doc["enabled"].GetBool();
}

ComponentSpotLight::ComponentSpotLight(GameObject* owner) : Component(owner) {}

void ComponentSpotLight::onEditor() {
    editorBaseLight(enabled, color, intensity);
    ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f);
    ImGui::DragFloat("Inner Angle", &innerAngle, 0.5f, 1.f, outerAngle);
    ImGui::DragFloat("Outer Angle", &outerAngle, 0.5f, innerAngle, 90.f);
    ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.f);
}

void ComponentSpotLight::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value dir, col;
    serializeVec3(doc, dir, direction);
    serializeVec3(doc, col, color);
    doc.AddMember("direction", dir, a);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("innerAngle", innerAngle, a);
    doc.AddMember("outerAngle", outerAngle, a);
    doc.AddMember("radius", radius, a);
    doc.AddMember("enabled", enabled, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentSpotLight::onLoad(const std::string& jsonStr) {
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("direction")) direction = deserializeVec3(doc["direction"]);
    if (doc.HasMember("color")) color = deserializeVec3(doc["color"]);
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("innerAngle")) innerAngle = doc["innerAngle"].GetFloat();
    if (doc.HasMember("outerAngle")) outerAngle = doc["outerAngle"].GetFloat();
    if (doc.HasMember("radius")) radius = doc["radius"].GetFloat();
    if (doc.HasMember("enabled")) enabled = doc["enabled"].GetBool();
}