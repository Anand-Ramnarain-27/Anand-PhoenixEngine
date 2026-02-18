#include "Globals.h"
#include "ComponentCamera.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <imgui.h>

using namespace rapidjson;

ComponentCamera::ComponentCamera(GameObject* owner)
    : Component(owner)
{
}

void ComponentCamera::onEditor()
{
    if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Checkbox("Main Camera", &m_isMainCamera);

    float fovDeg = m_fov * 57.2957795f;
    if (ImGui::SliderFloat("FOV", &fovDeg, 30.0f, 120.0f))
        m_fov = fovDeg * 0.0174532925f;

    ImGui::DragFloat("Near Plane", &m_nearPlane, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far Plane", &m_farPlane, 1.0f, 10.0f, 1000.0f);
    ImGui::ColorEdit4("Background Color", &m_backgroundColor.x);
    ImGui::Separator();

    if (auto* t = owner->getTransform())
        ImGui::Text("Position: %.2f, %.2f, %.2f", t->position.x, t->position.y, t->position.z);
}

void ComponentCamera::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();

    Value bg(kArrayType);
    bg.PushBack(m_backgroundColor.x, a).PushBack(m_backgroundColor.y, a)
        .PushBack(m_backgroundColor.z, a).PushBack(m_backgroundColor.w, a);

    doc.AddMember("FOV", m_fov, a);
    doc.AddMember("NearPlane", m_nearPlane, a);
    doc.AddMember("FarPlane", m_farPlane, a);
    doc.AddMember("IsMainCamera", m_isMainCamera, a);
    doc.AddMember("BackgroundColor", bg, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentCamera::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) { LOG("ComponentCamera: JSON parse error"); return; }

    if (doc.HasMember("FOV"))           m_fov = doc["FOV"].GetFloat();
    if (doc.HasMember("NearPlane"))     m_nearPlane = doc["NearPlane"].GetFloat();
    if (doc.HasMember("FarPlane"))      m_farPlane = doc["FarPlane"].GetFloat();
    if (doc.HasMember("IsMainCamera"))  m_isMainCamera = doc["IsMainCamera"].GetBool();
    if (doc.HasMember("BackgroundColor"))
    {
        const auto& c = doc["BackgroundColor"];
        m_backgroundColor = { c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat(), c[3].GetFloat() };
    }
}

Matrix ComponentCamera::getViewMatrix() const
{
    auto* t = owner->getTransform();
    return t ? t->getGlobalMatrix().Invert() : Matrix::Identity;
}

Matrix ComponentCamera::getProjectionMatrix(float aspectRatio) const
{
    return Matrix::CreatePerspectiveFieldOfView(m_fov, aspectRatio, m_nearPlane, m_farPlane);
}