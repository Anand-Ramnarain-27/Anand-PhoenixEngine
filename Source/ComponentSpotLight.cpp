#include "Globals.h"
#include "ComponentSpotLight.h"
#include "GameObject.h"
#include "ComponentTransform.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

#include <imgui.h>

using namespace rapidjson;

ComponentSpotLight::ComponentSpotLight(GameObject* owner)
    : ComponentLight(owner)
{
}

void ComponentSpotLight::onEditor()
{
    if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &m_enabled);
        ImGui::ColorEdit3("Color", &m_color.x);
        ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Radius", &m_radius, 0.1f, 0.1f, 100.0f);
        ImGui::DragFloat3("Direction", &m_direction.x, 0.01f);

        if (ImGui::Button("Normalize Direction"))
        {
            m_direction.Normalize();
        }

        ImGui::DragFloat("Inner Angle", &m_innerAngle, 0.5f, 0.0f, 90.0f);
        ImGui::DragFloat("Outer Angle", &m_outerAngle, 0.5f, 0.0f, 90.0f);

        if (m_innerAngle > m_outerAngle)
        {
            m_outerAngle = m_innerAngle;
        }

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

void ComponentSpotLight::onSave(std::string& outJson) const
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

    Value dirArray(kArrayType);
    dirArray.PushBack(m_direction.x, allocator);
    dirArray.PushBack(m_direction.y, allocator);
    dirArray.PushBack(m_direction.z, allocator);
    doc.AddMember("Direction", dirArray, allocator);

    doc.AddMember("InnerAngle", m_innerAngle, allocator);
    doc.AddMember("OuterAngle", m_outerAngle, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    outJson = buffer.GetString();
}

void ComponentSpotLight::onLoad(const std::string& jsonStr)
{
    Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LOG("ComponentSpotLight: JSON parse error");
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

    if (doc.HasMember("Direction"))
    {
        const Value& dir = doc["Direction"];
        m_direction = Vector3(
            dir[0].GetFloat(),
            dir[1].GetFloat(),
            dir[2].GetFloat());
    }

    if (doc.HasMember("InnerAngle"))
        m_innerAngle = doc["InnerAngle"].GetFloat();

    if (doc.HasMember("OuterAngle"))
        m_outerAngle = doc["OuterAngle"].GetFloat();
}