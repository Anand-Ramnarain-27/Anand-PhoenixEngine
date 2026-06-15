#include "Globals.h"
#include "ComponentDecal.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include <imgui.h>

ComponentDecal::ComponentDecal(GameObject* owner) : Component(owner){}

void ComponentDecal::onEditor(){
    ImGui::Checkbox("Enabled##decal", &enabled);
    ImGui::ColorEdit3("Colour", &colour.x);
    ImGui::SliderFloat("Opacity", &opacity, 0.f, 1.f);

    char buf[512] = {};
    strncpy_s(buf, texturePath.c_str(), sizeof(buf) - 1);
    if (ImGui::InputText("Texture", buf, sizeof(buf)))
        texturePath = buf;

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag a texture asset here or type a path.");
}

void ComponentDecal::onSave(std::string& outJson) const{
    outJson += "\"texturePath\":\"" + texturePath + "\",";
    outJson += "\"colour\":[" + std::to_string(colour.x) + "," +
                                 std::to_string(colour.y) + "," +
                                 std::to_string(colour.z) + "],";
    outJson += "\"opacity\":" + std::to_string(opacity) + ",";
    outJson += "\"enabled\":" + std::string(enabled ? "true" : "false");
}

void ComponentDecal::onLoad(const std::string& json){
    auto extract = [&](const char* key) -> std::string {
        std::string k = "\"" + std::string(key) + "\":";
        auto pos = json.find(k);
        if (pos == std::string::npos) return {};
        pos += k.size();
        if (json[pos] == '"'){
            ++pos;
            auto end = json.find('"', pos);
            return json.substr(pos, end - pos);
        }
        auto end = json.find_first_of(",}", pos);
        return json.substr(pos, end - pos);
    };

    texturePath = extract("texturePath");
    auto op = extract("opacity");
    if (!op.empty()) opacity = std::stof(op);
    auto en = extract("enabled");
    if (!en.empty()) enabled = (en == "true");
}
