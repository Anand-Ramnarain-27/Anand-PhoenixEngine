#include "Globals.h"
#include "GameViewPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleScene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "ModuleCamera.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include <functional>

GameViewPanel::GameViewPanel(ModuleEditor* editor) : ViewportPanel(editor){
    viewport.rt = std::make_unique<RenderTexture>("GameView", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.05f, 0.05f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
}

void GameViewPanel::draw(){
    bool playing = m_editor->getSceneManager() && m_editor->getSceneManager()->isPlaying();
    if (playing) ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.12f, 0.05f, 1.0f));
    ViewportPanel::draw();
    if (playing) ImGui::PopStyleColor();
}

// GAME VIEW — always uses active game camera from ModuleCamera
bool GameViewPanel::buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj){
    // The Game view always renders through ModuleCamera's centralized "active game
    // camera" pointer. This is independent of the editor/scene-view fly camera —
    // selecting an active game camera never touches the Scene viewport's matrices.
    GameObject* activeCamGO = app->getCamera()->getActiveCamera();
    ComponentCamera* cam = activeCamGO ? activeCamGO->getComponent<ComponentCamera>() : nullptr;
    ComponentTransform* t = activeCamGO ? activeCamGO->getTransform() : nullptr;

    if (cam && t) {
        m_usingFallbackCamera = false;
        Matrix world = t->getGlobalMatrix();
        Vector3 pos = world.Translation();
        Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
        Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();
        outView = Matrix::CreateLookAt(pos, pos + fwd, up);
        outProj = Matrix::CreatePerspectiveFieldOfView(cam->getFOV(), float(w) / float(h), cam->getNearPlane(), cam->getFarPlane());
        return true;
    }

    // Fallback: no active game camera set — show the editor fly-cam's view so the
    // Game View isn't left blank/stale, with a visible warning overlay.
    m_usingFallbackCamera = true;
    ModuleCamera* editorCam = app->getCamera();
    if (!editorCam) return false;
    outView = editorCam->getView();
    outProj = ModuleCamera::getPerspectiveProj(float(w) / float(h));
    return true;
}

void GameViewPanel::onDrawOverlays(){
    drawPlaymodeOverlay();
    drawNoActiveCameraOverlay();
}

void GameViewPanel::drawNoActiveCameraOverlay(){
    if (!m_usingFallbackCamera) return;
    ImGuiWindow* win = ImGui::FindWindowByName("Game View");
    if (!win) return;
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 10 },
        IM_COL32(255, 80, 80, 255), "No active game camera set");
}

void GameViewPanel::drawPlaymodeOverlay(){
    SceneManager* sm = m_editor->getSceneManager();
    if (!sm || !sm->isPlaying()) return;
    ImGuiWindow* win = ImGui::FindWindowByName("Game View");
    if (!win) return;
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 30 }, IM_COL32(80, 255, 80, 200), "PLAYING");
}
