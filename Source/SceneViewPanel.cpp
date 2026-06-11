#include "Globals.h"
#include "SceneViewPanel.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleCamera.h"
#include "ModuleScene.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "EditorSelection.h"
#include "Frustum.h"
#include "RenderTexture.h"
#include "PrefabManager.h"
#include "MousePicker.h"
#include <functional>

static constexpr float kDeg2Rad = 0.0174532925f;

SceneViewPanel::SceneViewPanel(ModuleEditor* editor) : ViewportPanel(editor){
    viewport.rt = std::make_unique<RenderTexture>("SceneView", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.1f, 0.1f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
}

void SceneViewPanel::draw(){
    if (m_fullscreen) {
        // On the first fullscreen frame, save the current dock node so we can
        // return to it when the user exits fullscreen.
        if (m_savedDockId == 0) {
            if (ImGuiWindow* win = ImGui::FindWindowByName("Viewport"))
                m_savedDockId = win->DockId;
        }

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y));
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always); // explicitly undock
        ImGui::SetNextWindowBgAlpha(1.f);
        constexpr ImGuiWindowFlags kFull =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin(getName(), &open, kFull)) drawContent();
        ImGui::End();
        ImGui::PopStyleVar();
    } else {
        // Exiting fullscreen — re-dock the window in its original node.
        if (m_savedDockId != 0) {
            ImGui::SetNextWindowDockID(m_savedDockId, ImGuiCond_Always);
            m_savedDockId = 0;
        }
        ViewportPanel::draw();
    }
}

bool SceneViewPanel::buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj){
    ModuleCamera* camera = app->getCamera();
    if (!camera) return false;
    // SCENE VIEW SKYBOX — uses editor fly-cam rotation, never game camera.
    // outView is forwarded to renderSceneWithCamera(), which draws the skybox
    // with this exact view matrix immediately, independent of the Game View.
    outView = camera->getView();
    outProj = ModuleCamera::getPerspectiveProj(float(w) / float(h));
    camera->clearGameCameraFrustum();
    if (ModuleScene* ms = m_editor->getActiveModuleScene()) {
        float aspect = float(w) / float(h);
        std::function<void(GameObject*)> findMainCam = [&](GameObject* go) {
            if (auto* cam = go->getComponent<ComponentCamera>(); cam && cam->isMainCamera())
                if (auto* t = go->getTransform()) {
                    Matrix world = t->getGlobalMatrix();
                    Vector3 pos = world.Translation();
                    Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
                    Vector3 right = Vector3::TransformNormal(Vector3::UnitX, world); right.Normalize();
                    Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();
                    camera->setGameCameraFrustum(Frustum::fromCamera(pos, fwd, right, up, cam->getFOV(), aspect, cam->getNearPlane(), cam->getFarPlane()));
                }
            for (auto* child : go->getChildren()) findMainCam(child);
            };
        findMainCam(ms->getRoot());
    }
    return true;
}

void SceneViewPanel::onPostRender(ID3D12GraphicsCommandList* cmd, uint32_t w, uint32_t h){
    BEGIN_EVENT(cmd, "Scene View");
    END_EVENT(cmd);
}

void SceneViewPanel::onResized(uint32_t w, uint32_t h){
    if (auto* sm = m_editor->getSceneManager()) sm->onViewportResized(w, h);
}

void SceneViewPanel::onImageDrawn(){
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragAsset)) m_editor->spawnAssetAtPath(std::string(static_cast<const char*>(payload->Data), payload->DataSize - 1));
        ImGui::EndDragDropTarget();
    }

    // Mouse picking: left-click on the viewport image while not dragging a gizmo.
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
        ModuleCamera* cam = app->getCamera();
        ModuleScene* ms = m_editor->getActiveModuleScene();
        if (cam && ms) {
            const float w = viewport.size.x, h = viewport.size.y;
            Matrix view = cam->getView();
            Matrix proj = ModuleCamera::getPerspectiveProj(w / h);

            ImVec2 mousePos = ImGui::GetIO().MousePos;
            GameObject* hit = MousePicker::pick(
                mousePos.x, mousePos.y,
                viewport.pos.x, viewport.pos.y, w, h,
                view, proj, ms);

            m_editor->getSelection().object = hit; // nullptr clears selection
        }
    }

    drawGizmo();
}

void SceneViewPanel::onDrawOverlays(){
    drawGizmoToolbar();
    drawPrefabExitButton();
    drawOverlay();
}

void SceneViewPanel::drawGizmoToolbar(){
    // Hotkeys
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) m_gizmoOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) m_gizmoOp = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_G)) m_gizmoMode = (m_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    // Render the toolbar directly in the current (Viewport) window so there
    // are no z-ordering issues with the docked panel.
    // Use viewport.pos (the ImGui::Image top-left, set in ViewportPanel::drawContent)
    // as the Y anchor.  This is always correct regardless of docking, tab-bar height,
    // or zero window padding, because it's the actual pixel origin of the rendered image.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cW = viewport.size.x;
    ImVec2 toolOrigin = ImVec2(viewport.pos.x, viewport.pos.y);

    const float btnSz = 22.f;
    const float toolH = btnSz + 8.f;
    const float padV = 4.f;

    // Semi-transparent toolbar strip drawn via DrawList (always on top of image)
    dl->AddRectFilled(
        ImVec2(toolOrigin.x, toolOrigin.y),
        ImVec2(toolOrigin.x + cW, toolOrigin.y + toolH),
        IM_COL32(18, 18, 22, 210));

    // Push tight style for toolbar buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.f, 2.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, 2.f));

    // Move cursor to toolbar area (overlaid on the image).
    // Must add a Dummy afterwards so ImGui registers the window boundary extension.
    ImGui::SetCursorScreenPos(ImVec2(toolOrigin.x + 6.f, toolOrigin.y + padV));

    // -- Gizmo op buttons (T/R/S) --
    auto gBtn = [&](const char* lbl, const char* tip, ImGuizmo::OPERATION op) {
        bool on = (m_gizmoOp == op);
        if (on) { ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::Acc);
                  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f,0.02f,0.14f,1.f)); }
        if (ImGui::Button(lbl, ImVec2(btnSz, btnSz))) m_gizmoOp = op;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        if (on) ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 2);
    };
    gBtn("T", "Translate (T)", ImGuizmo::TRANSLATE);
    gBtn("R", "Rotate    (R)", ImGuizmo::ROTATE);
    gBtn("S", "Scale     (S)", ImGuizmo::SCALE);

    // Divider
    ImGui::SameLine(0, 6);
    float divX = ImGui::GetCursorScreenPos().x - 3.f;
    dl->AddLine(ImVec2(divX, toolOrigin.y + 4.f), ImVec2(divX, toolOrigin.y + toolH - 4.f),
        ImGui::ColorConvertFloat4ToU32(EditorColors::Line2));
    ImGui::SameLine(0, 6);

    // -- Local / World toggle --
    bool local = (m_gizmoMode == ImGuizmo::LOCAL);
    if (local) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f,0.22f,0.32f,1.f));
    if (ImGui::Button(local ? "Local" : "World", ImVec2(46.f, btnSz)))
        m_gizmoMode = local ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Local / World space  (G)");
    if (local) ImGui::PopStyleColor();

    ImGui::SameLine(0, 4);

    // -- Snap toggle --
    if (m_useSnap) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f,0.22f,0.32f,1.f));
    if (ImGui::Button("Snap", ImVec2(36.f, btnSz))) m_useSnap = !m_useSnap;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid snap");
    if (m_useSnap) ImGui::PopStyleColor();

    // -- Transport: centred in the toolbar --
    {
        bool playing = m_editor->getSceneManager() && m_editor->getSceneManager()->isPlaying();
        bool paused = m_editor->getSceneManager() &&
                       m_editor->getSceneManager()->getState() == SceneManager::PlayState::Paused;
        const char* state = playing ? "PLAYING" : paused ? "PAUSED" : "EDIT";

        // Pre-measure the cluster so we can centre it.
        // 3 buttons + 2 gaps of 2px + 6px before badge + badge text
        const float clusterW = btnSz * 3.f + 2.f * 2.f + 6.f
                             + ImGui::CalcTextSize(state).x;

        // Current screen X after the left buttons
        float leftEndX = ImGui::GetItemRectMax().x;
        // Where the cluster should start to be centred over the full toolbar
        float centreX = toolOrigin.x + cW * 0.5f - clusterW * 0.5f;
        float spacer = centreX - leftEndX;
        ImGui::SameLine(0, spacer > 2.f ? spacer : 2.f);

        // ▶ Play
        if (playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f,0.44f,0.20f,1.f));
        else ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::Bg3);
        if (ImGui::Button("\xe2\x96\xb6##tb_play", ImVec2(btnSz, btnSz)) && !playing)
            m_editor->getSceneManager()->play();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2);

        // ⏸ Pause
        if (paused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f,0.36f,0.08f,1.f));
        else ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::Bg3);
        if (ImGui::Button("\xe2\x8f\xb8##tb_pause", ImVec2(btnSz, btnSz)) && playing)
            m_editor->getSceneManager()->pause();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2);

        // ⏹ Stop
        ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::Bg3);
        if (ImGui::Button("\xe2\x8f\xb9##tb_stop", ImVec2(btnSz, btnSz)))
            m_editor->stopPlay();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6);

        // State badge
        ImVec4 sCol = playing ? EditorColors::Ok : paused ? EditorColors::Warn : EditorColors::Tx2;
        ImGui::PushStyleColor(ImGuiCol_Text, sCol);
        ImGui::TextUnformatted(state);
        ImGui::PopStyleColor();
    }

    // -- Right side: Lit + Show + □ --
    // Use a spacer Dummy instead of SetCursorScreenPos to avoid the boundary assert.
    const float litW = 50.f, showW = 58.f, iconW = btnSz;
    const float rightGroupW = litW + showW + iconW + 4.f * 2.f;
    float currentScreenX = ImGui::GetItemRectMax().x;
    float targetScreenX = toolOrigin.x + cW - rightGroupW - 8.f;
    float spacer = targetScreenX - currentScreenX;
    if (spacer > 0.f) { ImGui::SameLine(0, spacer); }
    else { ImGui::SameLine(0, 4); }

    if (ImGui::Button("Lit v", ImVec2(litW, btnSz)))
        ImGui::OpenPopup("##lit_pp");
    if (ImGui::BeginPopup("##lit_pp")) {
        ImGui::SeparatorText("Shading Mode");
        ImGui::MenuItem("Lit", nullptr, true);
        ImGui::MenuItem("Unlit", nullptr, false);
        ImGui::MenuItem("Wireframe", nullptr, false);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0, 4);
    if (ImGui::Button("Show v", ImVec2(showW, btnSz)))
        ImGui::OpenPopup("##show_pp");
    if (ImGui::BeginPopup("##show_pp")) {
        SceneManager* sm = m_editor->getSceneManager();
        if (sm) {
            EditorSceneSettings& s = sm->getSettings();
            ImGui::MenuItem("Grid", nullptr, &s.showGrid);
            ImGui::MenuItem("Axis", nullptr, &s.showAxis);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Texture Sampler");
        ImGui::SetNextItemWidth(160.f);
        int samp = m_editor->getSamplerType();
        if (ImGui::Combo("##smp", &samp,
                "Linear / Wrap\0Point / Wrap\0Linear / Clamp\0Point / Clamp\0"))
            m_editor->setSamplerType(samp);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0, 4);
    // Capture state BEFORE the button so push/pop are always balanced.
    const bool wasFullscreen = m_fullscreen;
    if (wasFullscreen) ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::Acc);
    if (ImGui::Button(wasFullscreen ? "[x]" : "[ ]", ImVec2(iconW + 8.f, btnSz)))
        m_fullscreen = !m_fullscreen;
    if (wasFullscreen) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(wasFullscreen ? "Exit fullscreen  (click or Esc)" : "Fullscreen");
    // Esc exits fullscreen
    if (m_fullscreen && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        m_fullscreen = false;

    ImGui::PopStyleVar(2);

    // Extend the window's tracked content boundary to cover the full toolbar height.
    // This satisfies ImGui's requirement after SetCursorScreenPos and prevents the
    // white-screen assertion crash.
    ImGui::SetCursorScreenPos(ImVec2(toolOrigin.x, toolOrigin.y + toolH));
    ImGui::Dummy(ImVec2(cW, 1.f));
}

void SceneViewPanel::drawGizmo(){
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) return;
    ComponentTransform* t = sel.object->getTransform();
    if (!t) return;
    ModuleCamera* cam = app->getCamera();
    const float w = viewport.size.x, h = viewport.size.y;
    if (w <= 0 || h <= 0 || !cam) return;
    Matrix view = cam->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(w / h);
    Matrix world = t->getGlobalMatrix();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport.pos.x, viewport.pos.y, w, h);
    float snap[3] = {};
    float* snapPtr = nullptr;
    if (m_useSnap) {
        if (m_gizmoOp == ImGuizmo::TRANSLATE) { snap[0] = m_snapT[0]; snap[1] = m_snapT[1]; snap[2] = m_snapT[2]; }
        else if (m_gizmoOp == ImGuizmo::ROTATE) { snap[0] = m_snapR; }
        else { snap[0] = m_snapS; }
        snapPtr = snap;
    }
    if (ImGuizmo::Manipulate((const float*)&view, (const float*)&proj, m_gizmoOp, m_gizmoMode, (float*)&world, nullptr, snapPtr)) {
        Matrix local = world;
        if (GameObject* par = sel.object->getParent()) local = world * par->getTransform()->getGlobalMatrix().Invert();
        float tr[3], rot[3], sc[3];
        ImGuizmo::DecomposeMatrixToComponents((const float*)&local, tr, rot, sc);
        t->position = { tr[0], tr[1], tr[2] };
        t->scale = { sc[0], sc[1], sc[2] };
        t->rotation = Quaternion::CreateFromYawPitchRoll(rot[1] * kDeg2Rad, rot[0] * kDeg2Rad, rot[2] * kDeg2Rad);
        t->markDirty();
        GameObject* cur = sel.object;
        while (cur) {
            if (PrefabManager::isPrefabInstance(cur)) {
                PrefabManager::markPropertyOverride(cur, (int)Component::Type::Transform, "position");
                PrefabManager::markPropertyOverride(cur, (int)Component::Type::Transform, "rotation");
                PrefabManager::markPropertyOverride(cur, (int)Component::Type::Transform, "scale");
                break;
            }
            cur = cur->getParent();
        }
    }
}

void SceneViewPanel::drawOverlay(){
    ImGuiWindow* win = ImGui::FindWindowByName("Viewport");
    if (!win) return;
    char buf[160];
    sprintf_s(buf, "FPS: %.1f  CPU: %.2f ms  GPU: %.2f ms", app->getFPS(), app->getAvgElapsedMs(), m_editor->getGpuFrameTimeMs());
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + win->Size.y - 24 }, IM_COL32(0, 230, 0, 220), buf);
}

void SceneViewPanel::drawPrefabExitButton(){
    if (!m_editor->getSceneManager() || !m_editor->getSceneManager()->isEditingPrefab()) return;
    ImGuiWindow* win = ImGui::FindWindowByName("Viewport");
    if (!win) return;
    constexpr ImGuiWindowFlags kF = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    const std::string& name = m_editor->getSceneManager()->getPrefabEditName();
    const float btnW = 160.f;
    ImGui::SetNextWindowPos({ win->Pos.x + win->Size.x - btnW - 10.f, win->Pos.y + 28.f }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    if (!ImGui::Begin("##pfExitBtn", nullptr, kF)) { ImGui::End(); return; }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.f));
    ImGui::Text("Editing: %s", name.c_str());
    ImGui::PopStyleColor();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.12f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.18f, 0.18f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.07f, 0.07f, 1.f));
    if (ImGui::Button("Exit Prefab Edit  [Esc]", ImVec2(btnW, 0))) m_editor->exitPrefabEdit();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Leave without saving. Use Hierarchy right-click to Apply/Revert.");
    ImGui::End();
}
