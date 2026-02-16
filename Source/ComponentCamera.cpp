#include "Globals.h"
#include "ComponentCamera.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Application.h"
#include "ModuleCamera.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

#include <imgui.h>

using namespace rapidjson;

// Define default FOV (45 degrees in radians)
static constexpr float DEFAULT_FOV = 0.785398163f; // XM_PIDIV4 or 45 degrees

ComponentCamera::ComponentCamera(GameObject* owner)
    : Component(owner)
    , m_fov(DEFAULT_FOV)
    , m_nearPlane(0.1f)
    , m_farPlane(200.0f)
    , m_isMainCamera(false)
    , m_backgroundColor(0.2f, 0.3f, 0.4f, 1.0f)
{
}

void ComponentCamera::update(float deltaTime)
{
    // Camera logic can be handled here if needed
    // For now, ModuleCamera handles the main camera movement
}

void ComponentCamera::onEditor()
{
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Main Camera", &m_isMainCamera);

        // Convert FOV to degrees for easier editing
        float fovDegrees = m_fov * 57.2957795f; // Radians to degrees
        if (ImGui::SliderFloat("FOV", &fovDegrees, 30.0f, 120.0f))
        {
            m_fov = fovDegrees * 0.0174532925f; // Degrees to radians
        }

        ImGui::DragFloat("Near Plane", &m_nearPlane, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Far Plane", &m_farPlane, 1.0f, 10.0f, 1000.0f);

        ImGui::ColorEdit4("Background Color", &m_backgroundColor.x);

        ImGui::Separator();

        // Display current camera position/rotation from transform
        auto* transform = owner->getTransform();
        if (transform)
        {
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                transform->position.x,
                transform->position.y,
                transform->position.z);
        }
    }
}

void ComponentCamera::onSave(std::string& outJson) const
{
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    // Save camera settings
    doc.AddMember("FOV", m_fov, allocator);
    doc.AddMember("NearPlane", m_nearPlane, allocator);
    doc.AddMember("FarPlane", m_farPlane, allocator);
    doc.AddMember("IsMainCamera", m_isMainCamera, allocator);

    // Save background color
    Value bgColorArray(kArrayType);
    bgColorArray.PushBack(m_backgroundColor.x, allocator);
    bgColorArray.PushBack(m_backgroundColor.y, allocator);
    bgColorArray.PushBack(m_backgroundColor.z, allocator);
    bgColorArray.PushBack(m_backgroundColor.w, allocator);
    doc.AddMember("BackgroundColor", bgColorArray, allocator);

    // Convert to string
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    outJson = buffer.GetString();
}

void ComponentCamera::onLoad(const std::string& jsonStr)
{
    Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LOG("ComponentCamera: JSON parse error");
        return;
    }

    // Load camera settings
    if (doc.HasMember("FOV"))
        m_fov = doc["FOV"].GetFloat();

    if (doc.HasMember("NearPlane"))
        m_nearPlane = doc["NearPlane"].GetFloat();

    if (doc.HasMember("FarPlane"))
        m_farPlane = doc["FarPlane"].GetFloat();

    if (doc.HasMember("IsMainCamera"))
        m_isMainCamera = doc["IsMainCamera"].GetBool();

    // Load background color
    if (doc.HasMember("BackgroundColor"))
    {
        const Value& bgColor = doc["BackgroundColor"];
        m_backgroundColor = Vector4(
            bgColor[0].GetFloat(),
            bgColor[1].GetFloat(),
            bgColor[2].GetFloat(),
            bgColor[3].GetFloat()
        );
    }

    LOG("ComponentCamera: Loaded camera settings (FOV: %.2f degrees, Main: %d)",
        m_fov * 57.2957795f, m_isMainCamera);
}

Matrix ComponentCamera::getViewMatrix() const
{
    auto* transform = owner->getTransform();
    if (!transform)
        return Matrix::Identity;

    // Build view matrix from transform
    Matrix worldMatrix = transform->getGlobalMatrix();

    // Camera view is the inverse of the world transform
    return worldMatrix.Invert();
}

Matrix ComponentCamera::getProjectionMatrix(float aspectRatio) const
{
    return Matrix::CreatePerspectiveFieldOfView(
        m_fov,
        aspectRatio,
        m_nearPlane,
        m_farPlane
    );
}