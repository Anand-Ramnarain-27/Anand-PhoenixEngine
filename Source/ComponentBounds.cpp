#include "Globals.h"
#include "ComponentBounds.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentBounds::ComponentBounds(GameObject* owner) : Component(owner){}

void ComponentBounds::onEditor(){
    ImGui::SeparatorText("Shape");

    int typeIdx = (bvType == BVType::AABB) ? 0 : 1;
    if (ImGui::RadioButton("AABB (box)", &typeIdx, 0)) bvType = BVType::AABB;
    ImGui::SameLine();
    if (ImGui::RadioButton("Sphere", &typeIdx, 1)) bvType = BVType::Sphere;

    if (bvType == BVType::Sphere){
        ImGui::Spacing();
        ImGui::SeparatorText("Sphere Radius");
        if (radiusOverride < 0.f){
            ImGui::TextDisabled("Auto (derived from mesh AABB)");
            if (ImGui::Button("Override##rb")) radiusOverride = 1.f;
        } else {
            ImGui::DragFloat("Radius##rb", &radiusOverride, 0.01f, 0.001f, 1000.f, "%.3f");
            ImGui::SameLine();
            if (ImGui::SmallButton("Auto##rb")) radiusOverride = -1.f;
        }
    }
}

void ComponentBounds::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("bvType", (int)bvType, a);
    doc.AddMember("radiusOverride", radiusOverride, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentBounds::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("bvType"))
        bvType = static_cast<BVType>(doc["bvType"].GetInt());
    if (doc.HasMember("radiusOverride"))
        radiusOverride = doc["radiusOverride"].GetFloat();
}
