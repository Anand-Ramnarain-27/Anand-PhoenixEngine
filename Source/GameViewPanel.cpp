#include "Globals.h"
#include "GameViewPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleScene.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include "SceneManager.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "GameObject.h"

GameViewPanel::GameViewPanel(ModuleEditor* editor) : EditorPanel(editor)
{
    viewport.rt = std::make_unique<RenderTexture>(
        "GameView", DXGI_FORMAT_R8G8B8A8_UNORM,
        Vector4(0.05f, 0.05f, 0.1f, 1.0f),
        DXGI_FORMAT_D32_FLOAT, 1.0f);
}

void GameViewPanel::draw()
{
    SceneManager* sm = m_editor->getSceneManager();
    bool playing = sm && sm->isPlaying();

    if (playing)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.12f, 0.05f, 1.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Game View", &open,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        viewport.size = ImGui::GetContentRegionAvail();
        viewport.checkResize();

        if (viewport.isReady())
        {
            ImGui::Image((ImTextureID)viewport.rt->getSrvHandle().ptr, viewport.size);
            viewport.pos = ImGui::GetItemRectMin();
        }
        else
        {
            ImGui::TextDisabled("Game View not ready...");
        }

        drawPlaymodeOverlay();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (playing) ImGui::PopStyleColor();
}

void GameViewPanel::handleResize()
{
    if (!viewport.pendingResize) return;
    if (viewport.newWidth > 4 && viewport.newHeight > 4)
    {
        app->getD3D12()->flush();
        viewport.rt->resize(viewport.newWidth, viewport.newHeight);
    }
    viewport.pendingResize = false;
}

void GameViewPanel::drawPlaymodeOverlay()
{
    SceneManager* sm = m_editor->getSceneManager();
    if (!sm || !sm->isPlaying()) return;

    ImGuiWindow* win = ImGui::FindWindowByName("Game View");
    if (!win) return;

    ImGui::GetForegroundDrawList()->AddText(
        { win->Pos.x + 10, win->Pos.y + 30 },
        IM_COL32(80, 255, 80, 200), "PLAYING");
}

void GameViewPanel::renderToTexture(ID3D12GraphicsCommandList* cmd)
{
    const uint32_t w = (uint32_t)viewport.size.x;
    const uint32_t h = (uint32_t)viewport.size.y;
    if (!viewport.rt || w == 0 || h == 0) return;

    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return;

    ComponentCamera* mainCam = nullptr;
    Matrix camView, camProj;

    std::function<void(GameObject*)> findCam = [&](GameObject* go)
        {
            if (auto* c = go->getComponent<ComponentCamera>(); c && c->isMainCamera())
            {
                mainCam = c;
                auto* t = go->getTransform();
                if (t)
                {
                    Matrix world = t->getGlobalMatrix();
                    Vector3 pos = world.Translation();
                    Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
                    Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();
                    camView = Matrix::CreateLookAt(pos, pos + fwd, up);
                    camProj = Matrix::CreatePerspectiveFieldOfView(
                        c->getFOV(), float(w) / float(h), c->getNearPlane(), c->getFarPlane());
                }
            }
            if (!mainCam)
                for (auto* child : go->getChildren()) findCam(child);
        };
    findCam(scene->getRoot());

    if (!mainCam) return; 

    viewport.rt->beginRender(cmd);
    m_editor->renderSceneWithCamera(cmd, camView, camProj, w, h, false);
    viewport.rt->endRender(cmd);
}