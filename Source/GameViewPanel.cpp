#include "Globals.h"
#include "GameViewPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "SceneGraph.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "ModuleCamera.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include "ModuleUI.h"
#include "RuntimeCore.h"
#include <functional>

GameViewPanel::GameViewPanel(ModuleEditor* editor) : ViewportPanel(editor){
    viewport.rt = std::make_unique<RenderTexture>("GameView", kSceneColorFormat, Vector4(0.05f, 0.05f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
    viewport.rtScratch = std::make_unique<RenderTexture>("GameViewRtScratch", kSceneColorFormat, Vector4(0.05f, 0.05f, 0.1f, 1.0f));
    viewport.display = std::make_unique<RenderTexture>("GameViewDisplay", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.05f, 0.05f, 0.1f, 1.0f));
    viewport.displayScratch = std::make_unique<RenderTexture>("GameViewDisplayScratch", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.05f, 0.05f, 0.1f, 1.0f));
    for (int i = 0; i < EditorViewport::kNumBloomMips; ++i)
        viewport.bloomMips[i] = std::make_unique<RenderTexture>("GameViewBloomMip", kSceneColorFormat, Vector4(0.f, 0.f, 0.f, 1.0f));
}

void GameViewPanel::draw(){
    bool playing = m_editor->getSceneManager() && m_editor->getSceneManager()->isPlaying();
    if (playing) ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.12f, 0.05f, 1.0f));
    ViewportPanel::draw();
    if (playing) ImGui::PopStyleColor();
}

bool GameViewPanel::buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj){
    GameObject* activeCamGO = app->getCamera()->getActiveCamera();
    if (!activeCamGO) return false;
    auto* cam = activeCamGO->getComponent<ComponentCamera>();
    auto* t = activeCamGO->getTransform();
    if (!cam || !t) return false;

    Matrix world = t->getGlobalMatrix();
    Vector3 pos = world.Translation();
    Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
    Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();
    outView = Matrix::CreateLookAt(pos, pos + fwd, up);
    outProj = Matrix::CreatePerspectiveFieldOfView(cam->getFOV(), float(w) / float(h), cam->getNearPlane(), cam->getFarPlane());
    return true;
}

void GameViewPanel::onImageDrawn(){
    ModuleUI* ui = app->getUI();
    SceneManager* sm = m_editor->getSceneManager();
    // UI only reacts while the game runs; in edit mode widgets just sit there.
    if (!ui || !sm || !sm->isPlaying()) return;

    // Called right after the game image, so the last item is the image and the mouse maps 1:1 to its pixels.
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetMousePos();

    UIInput in;
    // A drag that started on a widget keeps tracking the pointer after it leaves the image.
    in.pointerValid = hovered || ImGui::IsMouseDown(ImGuiMouseButton_Left);
    in.pointer = Vector2(mouse.x - viewport.pos.x, mouse.y - viewport.pos.y);
    in.mousePressed = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    in.mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);   // even outside, so a held button lets go
    if (ImGui::IsWindowFocused()){
        in.tabPressed = ImGui::IsKeyPressed(ImGuiKey_Tab);
        in.shiftDown = ImGui::GetIO().KeyShift;
        in.submitPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
        in.submitReleased = ImGui::IsKeyReleased(ImGuiKey_Enter) || ImGui::IsKeyReleased(ImGuiKey_Space);
        in.navX = (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 1 : 0) - (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? 1 : 0);
        in.navY = (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ? 1 : 0) - (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : 0);
    }

    ui->updateInteraction(app->getRuntimeCore()->getActiveModuleScene(),
                          (uint32_t)viewport.size.x, (uint32_t)viewport.size.y, in);
}

void GameViewPanel::onDrawOverlays(){
    drawPlaymodeOverlay();

    if (!app->getCamera()->getActiveCamera()){
        ImGuiWindow* win = ImGui::FindWindowByName("Game View");
        if (win)
            ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 48 }, IM_COL32(255, 200, 80, 220),
                "No active camera - showing UI only (tick 'Is Active Camera' on a Camera)");
    }
}

void GameViewPanel::drawPlaymodeOverlay(){
    SceneManager* sm = m_editor->getSceneManager();
    if (!sm || !sm->isPlaying()) return;
    ImGuiWindow* win = ImGui::FindWindowByName("Game View");
    if (!win) return;
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 30 }, IM_COL32(80, 255, 80, 200), "PLAYING");
}
