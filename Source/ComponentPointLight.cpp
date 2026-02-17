#include "Globals.h"
#include "ComponentPointLight.h"
#include "GameObject.h"
#include "ComponentTransform.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

#include <imgui.h>

using namespace rapidjson;

ComponentPointLight::ComponentPointLight(GameObject* owner)
    : ComponentLight(owner)
{
}

void ComponentPointLight::onEditor()
{
    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &m_enabled);
        ImGui::ColorEdit3("Color", &m_color.x);
        ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Radius", &m_radius, 0.1f, 0.1f, 100.0f);

        auto* transform = getOwner()->getTransform();
        if (transform)
        {
            ImGui::Separator();
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                transform->position.x,
                transform->position.y,
                transform->position.z);
        }
    }
}

void ComponentPointLight::onSave(std::string& outJson) const
{
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("Enabled", m_enabled, allocator);

    Value colorArray(kArrayType);
    colorArray.PushBack(m_color.x, allocator);
    colorArray.PushBack(m_color.y, allocator);
    colorArray.PushBack(m_color.z, allocator);
    doc.AddMember("Color", colorArray, allocator);

    doc.AddMember("Intensity", m_intensity, allocator);
    doc.AddMember("Radius", m_radius, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    outJson = buffer.GetString();
}

void ComponentPointLight::onLoad(const std::string& jsonStr)
{
    Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LOG("ComponentPointLight: JSON parse error");
        return;
    }

    if (doc.HasMember("Enabled"))
        m_enabled = doc["Enabled"].GetBool();

    if (doc.HasMember("Color"))
    {
        const Value& color = doc["Color"];
        m_color = Vector3(
            color[0].GetFloat(),
            color[1].GetFloat(),
            color[2].GetFloat());
    }

    if (doc.HasMember("Intensity"))
        m_intensity = doc["Intensity"].GetFloat();

    if (doc.HasMember("Radius"))
        m_radius = doc["Radius"].GetFloat();
}