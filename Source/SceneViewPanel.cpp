#include "Globals.h"
#include "SceneViewPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleCamera.h"
#include "ModuleScene.h"
#include "RenderTexture.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "EditorSelection.h"
#include "Frustum.h"
#include <functional>

SceneViewPanel::SceneViewPanel(ModuleEditor* editor) : EditorPanel(editor)
{
    viewport.rt = std::make_unique<RenderTexture>(
        "SceneView", DXGI_FORMAT_R8G8B8A8_UNORM,
        Vector4(0.1f, 0.1f, 0.1f, 1.0f),
        DXGI_FORMAT_D32_FLOAT, 1.0f);
}

void SceneViewPanel::renderToTexture(ID3D12GraphicsCommandList* cmd)
{
    const uint32_t w = (uint32_t)viewport.size.x;
    const uint32_t h = (uint32_t)viewport.size.y;
    if (!viewport.rt || w == 0 || h == 0) return;

    ModuleCamera* camera = app->getCamera();
    if (!camera) return;

    const Matrix& view = camera->getView();
    Matrix        proj = ModuleCamera::getPerspectiveProj(float(w) / float(h));

    ModuleScene* moduleScene = m_editor->getActiveModuleScene();
    camera->clearGameCameraFrustum();
    if (moduleScene)
    {
        float aspect = float(w) / float(h);
        std::function<void(GameObject*)> findMainCam = [&](GameObject* go)
            {
                if (auto* cam = go->getComponent<ComponentCamera>(); cam && cam->isMainCamera())
                {
                    auto* t = go->getTransform();
                    if (t)
                    {
                        Matrix  world = t->getGlobalMatrix();
                        Vector3 pos = world.Translation();
                        Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
                        Vector3 right = Vector3::TransformNormal(Vector3::UnitX, world); right.Normalize();
                        Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();

                        camera->setGameCameraFrustum(Frustum::fromCamera(
                            pos, fwd, right, up,
                            cam->getFOV(), aspect,
                            cam->getNearPlane(), cam->getFarPlane()));
                    }
                }
                for (auto* child : go->getChildren()) findMainCam(child);
            };
        findMainCam(moduleScene->getRoot());
    }

    viewport.rt->beginRender(cmd);
    BEGIN_EVENT(cmd, "Scene View");
    m_editor->renderSceneWithCamera(cmd, view, proj, w, h, true);
    END_EVENT(cmd);
    viewport.rt->endRender(cmd);
}

void SceneViewPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Scene View", &open,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        viewport.size = ImGui::GetContentRegionAvail();
        viewport.checkResize();

        if (viewport.isReady())
        {
            ImGui::Image((ImTextureID)viewport.rt->getSrvHandle().ptr, viewport.size);
            viewport.pos = ImGui::GetItemRectMin();
            drawGizmo();
        }
        else
        {
            ImGui::TextDisabled("Scene View not ready...");
        }

        drawGizmoToolbar();
        drawOverlay();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void SceneViewPanel::handleResize()
{
    if (!viewport.pendingResize) return;
    if (viewport.newWidth > 4 && viewport.newHeight > 4)
    {
        app->getD3D12()->flush();
        viewport.rt->resize(viewport.newWidth, viewport.newHeight);
        if (auto* sm = m_editor->getSceneManager())
            sm->onViewportResized(viewport.newWidth, viewport.newHeight);
    }
    viewport.pendingResize = false;
}

void SceneViewPanel::drawGizmoToolbar()
{
    if (!ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) m_gizmoOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) m_gizmoOp = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_G))
            m_gizmoMode = (m_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    ImGuiWindow* win = ImGui::FindWindowByName("Scene View");
    if (!win) return;

    ImGui::SetNextWindowPos({ win->Pos.x + 8, win->Pos.y + 28 }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    constexpr ImGuiWindowFlags kF =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("##SceneGizmoBar", nullptr, kF)) { ImGui::End(); return; }

    auto btn = [&](const char* label, ImGuizmo::OPERATION op)
        {
            bool active = (m_gizmoOp == op);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1));
            if (ImGui::Button(label, ImVec2(40, 22))) m_gizmoOp = op;
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine(0, 2);
        };

    btn("T", ImGuizmo::TRANSLATE);
    btn("R", ImGuizmo::ROTATE);
    btn("S", ImGuizmo::SCALE);
    ImGui::SameLine(0, 8);

    bool local = (m_gizmoMode == ImGuizmo::LOCAL);
    if (ImGui::Button(local ? "Local" : "World", ImVec2(48, 22)))
        m_gizmoMode = local ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    ImGui::SameLine(0, 8);

    ImGui::Checkbox("Snap", &m_useSnap);
    ImGui::End();
}

void SceneViewPanel::drawGizmo()
{
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
    if (m_useSnap)
    {
        if (m_gizmoOp == ImGuizmo::TRANSLATE) { snap[0] = m_snapT[0]; snap[1] = m_snapT[1]; snap[2] = m_snapT[2]; }
        else if (m_gizmoOp == ImGuizmo::ROTATE) { snap[0] = m_snapR; }
        else { snap[0] = m_snapS; }
        snapPtr = snap;
    }

    if (ImGuizmo::Manipulate(
        (const float*)&view, (const float*)&proj,
        m_gizmoOp, m_gizmoMode, (float*)&world, nullptr, snapPtr))
    {
        Matrix local = world;
        if (GameObject* par = sel.object->getParent())
            local = world * par->getTransform()->getGlobalMatrix().Invert();

        float tr[3], rot[3], sc[3];
        ImGuizmo::DecomposeMatrixToComponents((const float*)&local, tr, rot, sc);
        t->position = { tr[0], tr[1], tr[2] };
        t->scale = { sc[0], sc[1], sc[2] };
        t->rotation = Quaternion::CreateFromYawPitchRoll(
            rot[1] * 0.0174532925f, rot[0] * 0.0174532925f, rot[2] * 0.0174532925f);
        t->markDirty();
    }
}

void SceneViewPanel::drawOverlay()
{
    ImGuiWindow* win = ImGui::FindWindowByName("Scene View");
    if (!win) return;
    char buf[160];
    sprintf_s(buf, "FPS: %.1f  CPU: %.2f ms  GPU: %.2f ms",
        app->getFPS(), app->getAvgElapsedMs(), m_editor->getGpuFrameTimeMs());
    ImGui::GetForegroundDrawList()->AddText(
        { win->Pos.x + 10, win->Pos.y + win->Size.y - 24 },
        IM_COL32(0, 230, 0, 220), buf);
}