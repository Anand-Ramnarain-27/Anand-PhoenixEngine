#include "Globals.h"
#include "ComponentCamera.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Application.h"
#include "ModuleCamera.h"
#include "ModuleEditor.h"
#include "EditorColors.h"
#include "SceneGraph.h"
#include <functional>
#include <vector>
#include <algorithm>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <imgui.h>

using namespace rapidjson;

ComponentCamera::ComponentCamera(GameObject* owner) : Component(owner){}

void ComponentCamera::update(float){
    rebuildFrustum();
    if (isMainCamera()) app->getCamera()->setGameCameraFrustum(m_frustum);
}

bool ComponentCamera::isMainCamera() const{
    return app->getCamera()->getActiveCamera() == owner;
}

void ComponentCamera::setMainCamera(bool v){
    ModuleCamera* cam = app->getCamera();
    if (v){
        cam->setActiveCamera(owner);
    } else if (cam->getActiveCamera() == owner){
        cam->setActiveCamera(nullptr);
        cam->clearGameCameraFrustum();
    }
}

void ComponentCamera::rebuildFrustum(){
    auto* t = owner->getTransform();
    if (!t) return;
    const Matrix& world = t->getGlobalMatrix();
    Vector3 right(world._11, world._12, world._13);
    Vector3 up(world._21, world._22, world._23);
    Vector3 forward(-world._31, -world._32, -world._33); // camera looks down -Z
    right.Normalize(); up.Normalize(); forward.Normalize();
    m_frustum = Frustum::fromCamera(world.Translation(), forward, right, up, m_fov, app->getCamera()->aspectRatio, m_nearPlane, m_farPlane);
}

static constexpr float kDeg2Rad = 0.0174532925f;
static constexpr float kRad2Deg = 57.2957795f;

void ComponentCamera::onEditor(){
    ComponentCamera* cam = this;
    bool isMain = cam->isMainCamera();
    if (ImGui::Checkbox("Is Active Camera", &isMain)) cam->setMainCamera(isMain);
    if (isMain){ ImGui::SameLine(); ImGui::TextColored(EditorColors::Success, "(rendering camera)"); }

    ModuleCamera* modCam = app->getCamera();
    bool isCulling = (modCam->cullSource == ModuleCamera::CullSource::GameCamera);
    if (ImGui::Checkbox("Is Culling Camera", &isCulling)) modCam->cullSource = isCulling ? ModuleCamera::CullSource::GameCamera : ModuleCamera::CullSource::EditorCamera;
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("When checked, this camera's frustum is used\nfor frustum culling instead of the editor camera.");
    ImGui::Separator();

    float fovDeg = cam->getFOV() * kRad2Deg;
    if (ImGui::SliderFloat("Field of View", &fovDeg, 10.0f, 170.0f)) cam->setFOV(fovDeg * kDeg2Rad);

    float nearP = cam->getNearPlane(), farP = cam->getFarPlane();
    if (ImGui::DragFloat("Near Plane", &nearP, 0.01f, 0.001f, farP - 0.01f)) cam->setNearPlane(nearP);
    if (ImGui::DragFloat("Far Plane", &farP, 1.0f, nearP + 0.01f, 10000.f)) cam->setFarPlane(farP);

    ImVec2 svSize = app->getEditor()->getSceneViewSize();
    if (svSize.x > 0 && svSize.y > 0) ImGui::Text("Aspect Ratio: %.3f  (%dx%d)", svSize.x / svSize.y, (int)svSize.x, (int)svSize.y);
    ImGui::Separator();

    Vector4 bg = cam->getBackgroundColor();
    if (ImGui::ColorEdit4("Background", &bg.x)) cam->setBackgroundColor(bg);
    ImGui::Separator();

    SceneGraph* scene = app->getEditor()->getActiveModuleScene();
    if (!scene) return;

    struct CamEntry { GameObject* go; ComponentCamera* cam; };
    std::vector<CamEntry> allCams;
    std::function<void(GameObject*)> collect = [&](GameObject* node){
        if (auto* c = node->getComponent<ComponentCamera>()) allCams.push_back({ node, c });
        for (auto* child : node->getChildren()) collect(child);
        };
    collect(scene->getRoot());
    if (allCams.empty()) return;

    ImGui::TextDisabled("Scene cameras:");
    float listH = std::min<float>(allCams.size() * 22.0f + 8.0f, 120.0f);
    if (ImGui::BeginChild("##camList", ImVec2(0, listH), true)){
        for (auto& e : allCams){
            bool isThis = (e.cam == cam);
            bool isMain2 = e.cam->isMainCamera();
            std::string lbl = isMain2 ? "[Main] " + e.go->getName() : e.go->getName();
            ImGui::PushID(e.go->getUID());
            if (isThis) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
            if (ImGui::Selectable(lbl.c_str(), isMain2)){
                for (auto& e2 : allCams) e2.cam->setMainCamera(false);
                e.cam->setMainCamera(true);
                app->getEditor()->getSelection().object = e.go;
            }
            if (isThis) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()){
                ImGui::BeginTooltip();
                ImGui::Text("GameObject: %s", e.go->getName().c_str());
                ImGui::Text("FOV: %.1f deg", e.cam->getFOV() * kRad2Deg);
                ImGui::Text("Near: %.3f  Far: %.1f", e.cam->getNearPlane(), e.cam->getFarPlane());
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (!cam->isMainCamera())
        if (ImGui::Button("Make Active Camera")){
            for (auto& e : allCams) e.cam->setMainCamera(false);
            cam->setMainCamera(true);
        }
}

void ComponentCamera::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value bg(kArrayType);
    bg.PushBack(m_backgroundColor.x, a).PushBack(m_backgroundColor.y, a).PushBack(m_backgroundColor.z, a).PushBack(m_backgroundColor.w, a);
    doc.AddMember("FOV", m_fov, a);
    doc.AddMember("NearPlane", m_nearPlane, a);
    doc.AddMember("FarPlane", m_farPlane, a);
    doc.AddMember("IsMainCamera", isMainCamera(), a);
    doc.AddMember("BackgroundColor", bg, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentCamera::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()){ LOG("ComponentCamera: JSON parse error"); return; }
    if (doc.HasMember("FOV")) m_fov = doc["FOV"].GetFloat();
    if (doc.HasMember("NearPlane")) m_nearPlane = doc["NearPlane"].GetFloat();
    if (doc.HasMember("FarPlane")) m_farPlane = doc["FarPlane"].GetFloat();
    if (doc.HasMember("IsMainCamera") && doc["IsMainCamera"].GetBool()) setMainCamera(true);
    if (doc.HasMember("BackgroundColor")){
        const auto& c = doc["BackgroundColor"];
        m_backgroundColor = { c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat(), c[3].GetFloat() };
    }
}

Matrix ComponentCamera::getViewMatrix() const{
    auto* t = owner->getTransform();
    return t ? t->getGlobalMatrix().Invert() : Matrix::Identity;
}

Matrix ComponentCamera::getProjectionMatrix(float aspectRatio) const{
    return Matrix::CreatePerspectiveFieldOfView(m_fov, aspectRatio, m_nearPlane, m_farPlane);
}
