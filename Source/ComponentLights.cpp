#include "Globals.h"
#include "ComponentLights.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

namespace {

    void serializeVec3(Document& doc, Value& arr, const Vector3& v){
        auto& a = doc.GetAllocator();
        arr.SetArray();
        arr.PushBack(v.x, a).PushBack(v.y, a).PushBack(v.z, a);
    }

    Vector3 deserializeVec3(const Value& arr){
        return { arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat() };
    }

    void editorBaseLight(bool& enabled, Vector3& color, float& intensity){
        ImGui::Checkbox("Enabled", &enabled);
        ImGui::ColorEdit3("Color", &color.x);
        ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.f, 10.f);
    }

}

ComponentDirectionalLight::ComponentDirectionalLight(GameObject* owner) : Component(owner){}

void ComponentDirectionalLight::onEditor(){
    editorBaseLight(enabled, color, intensity);
    ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f);

    ImGui::SeparatorText("Shadows");
    ImGui::Checkbox("Cast Shadows", &castShadows);
    if (castShadows){
        const char* resItems[] = { "512", "1024", "2048", "4096" };
        const int resValues[] = { 512, 1024, 2048, 4096 };
        int resIdx = 2;
        for (int i = 0; i < 4; ++i) if (resValues[i] == shadowResolution) resIdx = i;
        if (ImGui::Combo("Resolution", &resIdx, resItems, 4)) shadowResolution = resValues[resIdx];

        const char* modeItems[] = { "PCF", "VSM (Variance)", "ESM (Exponential)" };
        ImGui::Combo("Filter Mode", &shadowMode, modeItems, 3);
        ImGui::SetItemTooltip("PCF = hardware compare kernel; VSM/ESM = pre-filtered moments.");

        ImGui::DragFloat("Bias", &shadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
        ImGui::SetItemTooltip("Too low -> shadow acne. Too high -> Peter panning.");
        if (shadowMode == 0){
            ImGui::DragFloat("PCF Radius", &shadowPcfRadius, 0.1f, 0.f, 4.f, "%.0f");
            ImGui::SetItemTooltip("Soft-shadow kernel half-width in texels (0 = hard edge).");
        } else {
            ImGui::DragFloat("Light Bleed Reduction", &shadowLightBleed, 0.01f, 0.f, 0.95f);
            ImGui::SetItemTooltip("Darkens partial shadows to fight VSM/ESM light leaking.");
            if (shadowMode == 2)
                ImGui::DragFloat("ESM Exponent (k)", &shadowExpK, 0.5f, 1.f, 90.f);
        }
        ImGui::DragFloat("Shadow Distance", &shadowDistance, 1.0f, 5.f, 500.f);
        ImGui::SetItemTooltip("Closer = sharper shadows (less wasted resolution / aliasing).");
        ImGui::DragFloat("Sun Distance", &shadowSunDistance, 1.0f, 1.f, 500.f);
        ImGui::SliderFloat("Ambient Darkening", &shadowAmbientStrength, 0.f, 1.f);
        ImGui::SetItemTooltip("Darken IBL/ambient in shadow so ground shadows show under a skybox.");

        ImGui::SliderInt("Cascades", &shadowCascadeCount, 1, 4);
        ImGui::SetItemTooltip("Z-partition the view into N shadow maps (Lecture 16).");
        ImGui::SliderFloat("Cascade Blend", &shadowCascadeLambda, 0.f, 1.f);
        ImGui::SetItemTooltip("Linear (0) vs logarithmic (1) split distribution.");
        ImGui::Checkbox("Debug Cascades", &shadowDebugCascades);
        ImGui::SetItemTooltip("Tint each cascade region a different colour.");
        ImGui::Checkbox("GPU Frustum (parallel reduction)", &shadowGpuFrustum);
        ImGui::SetItemTooltip("Phase 5: build a single tight light frustum on the GPU from the\n"
                              "depth buffer (1-frame latency). Overrides cascades when on.");
    }
}

void ComponentDirectionalLight::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value dir, col;
    serializeVec3(doc, dir, direction);
    serializeVec3(doc, col, color);
    doc.AddMember("direction", dir, a);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("enabled", enabled, a);
    doc.AddMember("castShadows", castShadows, a);
    doc.AddMember("shadowResolution", shadowResolution, a);
    doc.AddMember("shadowBias", shadowBias, a);
    doc.AddMember("shadowPcfRadius", shadowPcfRadius, a);
    doc.AddMember("shadowDistance", shadowDistance, a);
    doc.AddMember("shadowSunDistance", shadowSunDistance, a);
    doc.AddMember("shadowMode", shadowMode, a);
    doc.AddMember("shadowExpK", shadowExpK, a);
    doc.AddMember("shadowLightBleed", shadowLightBleed, a);
    doc.AddMember("shadowCascadeCount", shadowCascadeCount, a);
    doc.AddMember("shadowCascadeLambda", shadowCascadeLambda, a);
    doc.AddMember("shadowDebugCascades", shadowDebugCascades, a);
    doc.AddMember("shadowGpuFrustum", shadowGpuFrustum, a);
    doc.AddMember("shadowAmbientStrength", shadowAmbientStrength, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentDirectionalLight::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("direction")) direction = deserializeVec3(doc["direction"]);
    if (doc.HasMember("color")) color = deserializeVec3(doc["color"]);
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("enabled")) enabled = doc["enabled"].GetBool();
    if (doc.HasMember("castShadows")) castShadows = doc["castShadows"].GetBool();
    if (doc.HasMember("shadowResolution")) shadowResolution = doc["shadowResolution"].GetInt();
    if (doc.HasMember("shadowBias")) shadowBias = doc["shadowBias"].GetFloat();
    if (doc.HasMember("shadowPcfRadius")) shadowPcfRadius = doc["shadowPcfRadius"].GetFloat();
    if (doc.HasMember("shadowDistance")) shadowDistance = doc["shadowDistance"].GetFloat();
    if (doc.HasMember("shadowSunDistance")) shadowSunDistance = doc["shadowSunDistance"].GetFloat();
    if (doc.HasMember("shadowMode")) shadowMode = doc["shadowMode"].GetInt();
    if (doc.HasMember("shadowExpK")) shadowExpK = doc["shadowExpK"].GetFloat();
    if (doc.HasMember("shadowLightBleed")) shadowLightBleed = doc["shadowLightBleed"].GetFloat();
    if (doc.HasMember("shadowCascadeCount")) shadowCascadeCount = doc["shadowCascadeCount"].GetInt();
    if (doc.HasMember("shadowCascadeLambda")) shadowCascadeLambda = doc["shadowCascadeLambda"].GetFloat();
    if (doc.HasMember("shadowDebugCascades")) shadowDebugCascades = doc["shadowDebugCascades"].GetBool();
    if (doc.HasMember("shadowGpuFrustum")) shadowGpuFrustum = doc["shadowGpuFrustum"].GetBool();
    if (doc.HasMember("shadowAmbientStrength")) shadowAmbientStrength = doc["shadowAmbientStrength"].GetFloat();
}

ComponentPointLight::ComponentPointLight(GameObject* owner) : Component(owner){}

void ComponentPointLight::onEditor(){
    editorBaseLight(enabled, color, intensity);
    ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.f);

    ImGui::SeparatorText("Shadows");
    ImGui::Checkbox("Cast Shadows", &castShadows);
    if (castShadows){
        const char* resItems[] = { "512", "1024", "2048" };
        const int resValues[] = { 512, 1024, 2048 };
        int resIdx = 1;
        for (int i = 0; i < 3; ++i) if (resValues[i] == shadowResolution) resIdx = i;
        if (ImGui::Combo("Cube Resolution", &resIdx, resItems, 3)) shadowResolution = resValues[resIdx];
        ImGui::DragFloat("Bias", &shadowBias, 0.001f, 0.f, 0.2f, "%.3f");
        ImGui::SetItemTooltip("Cubemap shadow: 6 perspective faces store distance to the light.");
    }
}

void ComponentPointLight::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value col;
    serializeVec3(doc, col, color);
    doc.AddMember("color", col, a);
    doc.AddMember("intensity", intensity, a);
    doc.AddMember("radius", radius, a);
    doc.AddMember("enabled", enabled, a);
    doc.AddMember("castShadows", castShadows, a);
    doc.AddMember("shadowResolution", shadowResolution, a);
    doc.AddMember("shadowBias", shadowBias, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentPointLight::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("color")) color = deserializeVec3(doc["color"]);
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("radius")) radius = doc["radius"].GetFloat();
    if (doc.HasMember("enabled")) enabled = doc["enabled"].GetBool();
    if (doc.HasMember("castShadows")) castShadows = doc["castShadows"].GetBool();
    if (doc.HasMember("shadowResolution")) shadowResolution = doc["shadowResolution"].GetInt();
    if (doc.HasMember("shadowBias")) shadowBias = doc["shadowBias"].GetFloat();
}

ComponentSpotLight::ComponentSpotLight(GameObject* owner) : Component(owner){}

void ComponentSpotLight::onEditor(){
    editorBaseLight(enabled, color, intensity);
    ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f);
    ImGui::DragFloat("Inner Angle", &innerAngle, 0.5f, 1.f, outerAngle);
    ImGui::DragFloat("Outer Angle", &outerAngle, 0.5f, innerAngle, 90.f);
    ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.f);

    ImGui::SeparatorText("Shadows");
    ImGui::Checkbox("Cast Shadows", &castShadows);
    if (castShadows){
        const char* resItems[] = { "512", "1024", "2048", "4096" };
        const int resValues[] = { 512, 1024, 2048, 4096 };
        int resIdx = 1;
        for (int i = 0; i < 4; ++i) if (resValues[i] == shadowResolution) resIdx = i;
        if (ImGui::Combo("Resolution", &resIdx, resItems, 4)) shadowResolution = resValues[resIdx];
        ImGui::DragFloat("Bias", &shadowBias, 0.0001f, 0.f, 0.05f, "%.4f");
        ImGui::DragFloat("PCF Radius", &shadowPcfRadius, 0.1f, 0.f, 4.f, "%.0f");
    }
}

void ComponentSpotLight::onSave(std::string& outJson) const{
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
    doc.AddMember("castShadows", castShadows, a);
    doc.AddMember("shadowResolution", shadowResolution, a);
    doc.AddMember("shadowBias", shadowBias, a);
    doc.AddMember("shadowPcfRadius", shadowPcfRadius, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentSpotLight::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("direction")) direction = deserializeVec3(doc["direction"]);
    if (doc.HasMember("color")) color = deserializeVec3(doc["color"]);
    if (doc.HasMember("intensity")) intensity = doc["intensity"].GetFloat();
    if (doc.HasMember("innerAngle")) innerAngle = doc["innerAngle"].GetFloat();
    if (doc.HasMember("outerAngle")) outerAngle = doc["outerAngle"].GetFloat();
    if (doc.HasMember("radius")) radius = doc["radius"].GetFloat();
    if (doc.HasMember("enabled")) enabled = doc["enabled"].GetBool();
    if (doc.HasMember("castShadows")) castShadows = doc["castShadows"].GetBool();
    if (doc.HasMember("shadowResolution")) shadowResolution = doc["shadowResolution"].GetInt();
    if (doc.HasMember("shadowBias")) shadowBias = doc["shadowBias"].GetFloat();
    if (doc.HasMember("shadowPcfRadius")) shadowPcfRadius = doc["shadowPcfRadius"].GetFloat();
}
