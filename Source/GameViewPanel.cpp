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
#include "EditorSceneSettings.h"
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
        // Gamepad (player 0), alongside the keyboard: shoulder buttons cycle focus like Tab/Shift+Tab, D-pad
        // nudges a focused slider or caret like the arrow keys, A submits like Enter/Space, B drops focus like
        // Escape. Read via ModuleInput (updated regardless of editor/player), not ImGui.
        using Phoenix::GamepadButton;
        ModuleInput* input = app->getInput();
        const bool padNext = input->isButtonPressed(GamepadButton::RightShoulder, 0);
        const bool padPrev = input->isButtonPressed(GamepadButton::LeftShoulder, 0);
        const bool padRight = input->isButtonPressed(GamepadButton::DPadRight, 0);
        const bool padLeft = input->isButtonPressed(GamepadButton::DPadLeft, 0);
        const bool padUp = input->isButtonPressed(GamepadButton::DPadUp, 0);
        const bool padDown = input->isButtonPressed(GamepadButton::DPadDown, 0);

        in.tabPressed = ImGui::IsKeyPressed(ImGuiKey_Tab) || padNext || padPrev;
        in.shiftDown = ImGui::GetIO().KeyShift || padPrev;
        in.submitPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) || input->isButtonPressed(GamepadButton::A, 0);
        in.submitReleased = ImGui::IsKeyReleased(ImGuiKey_Enter) || ImGui::IsKeyReleased(ImGuiKey_Space) || input->isButtonReleased(GamepadButton::A, 0);
        in.navX = (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || padRight ? 1 : 0) - (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || padLeft ? 1 : 0);
        in.navY = (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || padUp ? 1 : 0) - (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || padDown ? 1 : 0);

        // Text entry: ImGui's platform backend already turned WM_CHAR into this queue.
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < io.InputQueueCharacters.Size; ++i){
            const ImWchar c = io.InputQueueCharacters[i];
            if (c >= 32 && c < 127) in.text += static_cast<char>(c);
        }
        in.backspace = ImGui::IsKeyPressed(ImGuiKey_Backspace) ? 1 : 0;
        in.deleteKey = ImGui::IsKeyPressed(ImGuiKey_Delete) ? 1 : 0;
        in.home = ImGui::IsKeyPressed(ImGuiKey_Home, false);
        in.end = ImGui::IsKeyPressed(ImGuiKey_End, false);
        in.enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
        in.escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || input->isButtonPressed(GamepadButton::B, 0);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
            if (const char* clip = ImGui::GetClipboardText()) in.paste = clip;
        in.selectAllPressed = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false);
        in.copyPressed = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false);
        in.cutPressed = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X, false);
    }

    ui->updateInteraction(app->getRuntimeCore()->getActiveModuleScene(),
                          (uint32_t)viewport.size.x, (uint32_t)viewport.size.y, in);
}

void GameViewPanel::onDrawOverlays(){
    drawPlaymodeOverlay();
    drawUIDebugRects();

    if (!app->getCamera()->getActiveCamera()){
        ImGuiWindow* win = ImGui::FindWindowByName("Game View");
        if (win)
            ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 48 }, IM_COL32(255, 200, 80, 220),
                "No active camera - showing UI only (tick 'Is Active Camera' on a Camera)");
    }
}

// "UI Rects / Anchors" debug toggle (Menu > Debug): outlines every ComponentTransform2D's rect, marks its
// pivot, and marks where its anchors sit in the parent rect. Runs every frame regardless of play state, since
// it is meant for authoring, not just for watching the game run.
void GameViewPanel::drawUIDebugRects(){
    ModuleUI* ui = app->getUI();
    SceneManager* sm = m_editor->getSceneManager();
    if (!ui || !sm || !sm->getSettings().debugDrawUIRects) return;
    if (!viewport.display || !viewport.display->isValid()) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = viewport.pos;
    const ImU32 rectColor = IM_COL32(80, 200, 255, 200);
    const ImU32 pivotColor = IM_COL32(255, 220, 60, 230);
    const ImU32 anchorColor = IM_COL32(255, 110, 220, 220);

    auto toScreen = [&](const Vector2& p){ return ImVec2(origin.x + p.x, origin.y + p.y); };
    auto drawAnchorHandle = [&](ImVec2 p){
        dl->AddTriangleFilled(ImVec2(p.x, p.y - 6.f), ImVec2(p.x - 5.f, p.y + 5.f), ImVec2(p.x + 5.f, p.y + 5.f), anchorColor);
    };

    for (const ModuleUI::UIDebugRect& r : ui->getDebugRects()){
        ImVec2 pts[4];
        for (int i = 0; i < 4; ++i) pts[i] = toScreen(r.corners[i]);
        dl->AddLine(pts[0], pts[1], rectColor, 1.5f);
        dl->AddLine(pts[1], pts[2], rectColor, 1.5f);
        dl->AddLine(pts[2], pts[3], rectColor, 1.5f);
        dl->AddLine(pts[3], pts[0], rectColor, 1.5f);

        const ImVec2 pv = toScreen(r.pivotPx);
        dl->AddQuadFilled(ImVec2(pv.x, pv.y - 5.f), ImVec2(pv.x + 5.f, pv.y), ImVec2(pv.x, pv.y + 5.f), ImVec2(pv.x - 5.f, pv.y), pivotColor);

        drawAnchorHandle(toScreen(r.anchorMinPx));
        if (r.stretched) drawAnchorHandle(toScreen(r.anchorMaxPx));
    }
}

void GameViewPanel::drawPlaymodeOverlay(){
    SceneManager* sm = m_editor->getSceneManager();
    if (!sm || !sm->isPlaying()) return;
    ImGuiWindow* win = ImGui::FindWindowByName("Game View");
    if (!win) return;
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + 30 }, IM_COL32(80, 255, 80, 200), "PLAYING");
}
