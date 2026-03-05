#include "Globals.h"
#include "GameViewPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleScene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include <functional>

GameViewPanel::GameViewPanel(ModuleEditor* editor) : ViewportPanel(editor)
{
    viewport.rt = std::make_unique<RenderTexture>(
        "GameView", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.05f, 0.05f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
}

void GameViewPanel::draw()
{
    bool playing = m_editor->getSceneManager() && m_editor->getSceneManager()->isPlaying();
    if (playing) ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.12f, 0.05f, 1.0f));
    ViewportPanel::draw();
    if (playing) ImGui::PopStyleColor();
}

bool GameViewPanel::buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj)
{
    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return false;

    ComponentCamera* mainCam = nullptr;

    std::function<void(GameObject*)> findCam = [&](GameObject* go)
        {
            if (auto* c = go->getComponent<ComponentCamera>(); c && c->isMainCamera())
            {
                mainCam = c;
                if (auto* t = go->getTransform())
                {
                    Matrix world = t->getGlobalMatrix();
                    Vector3 pos = world.Translation();
                    Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
                    Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();
                    outView = Matrix::CreateLookAt(pos, pos + fwd, up);
                    outProj = Matrix::CreatePerspectiveFieldOfView(c->getFOV(), float(w) / float(h), c->getNearPlane(), c->getFarPlane());
                }
            }
            if (!mainCam) for (auto* child : go->getChildren()) findCam(child);
        };
    findCam(scene->getRoot());
    return mainCam != nullptr;
}

void GameViewPanel::onDrawOverlays()
{
    drawPlaymodeOverlay();
}

void GameViewPanel::drawPlaymodeOverlay()
{
    SceneManager* sm = m_editor->getSceneManager();
    if (!sm || !sm->isPlaying()) return;
    ImGuiWindow* win = ImGui::FindWindowByName("Game View");
    if (!win) return;
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 30 }, IM_COL32(80, 255, 80, 200), "PLAYING");
}