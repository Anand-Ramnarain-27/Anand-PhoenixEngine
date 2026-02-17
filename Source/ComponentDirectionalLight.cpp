#include "Globals.h"
#include "ComponentDirectionalLight.h"
#include "GameObject.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

#include <imgui.h>

using namespace rapidjson;

ComponentDirectionalLight::ComponentDirectionalLight(GameObject* owner)
    : ComponentLight(owner)
{
}

void ComponentDirectionalLight::onEditor()
{
    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled", &m_enabled);
        ImGui::ColorEdit3("Color", &m_color.x);
        ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat3("Direction", &m_direction.x, 0.01f);

        if (ImGui::Button("Normalize Direction"))
        {
            m_direction.Normalize();
        }
    }
}

void ComponentDirectionalLight::onSave(std::string& outJson) const
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

    Value dirArray(kArrayType);
    dirArray.PushBack(m_direction.x, allocator);
    dirArray.PushBack(m_direction.y, allocator);
    dirArray.PushBack(m_direction.z, allocator);
    doc.AddMember("Direction", dirArray, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    outJson = buffer.GetString();
}

void ComponentDirectionalLight::onLoad(const std::string& jsonStr)
{
    Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LOG("ComponentDirectionalLight: JSON parse error");
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

    if (doc.HasMember("Direction"))
    {
        const Value& dir = doc["Direction"];
        m_direction = Vector3(
            dir[0].GetFloat(),
            dir[1].GetFloat(),
            dir[2].GetFloat());
    }
}