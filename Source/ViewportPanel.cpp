#include "Globals.h"
#include "ViewportPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"

void ViewportPanel::renderToTexture(ID3D12GraphicsCommandList* cmd)
{
    const uint32_t w = (uint32_t)viewport.size.x;
    const uint32_t h = (uint32_t)viewport.size.y;
    if (!viewport.rt || w == 0 || h == 0) return;

    Matrix view, proj;
    if (!buildCameraMatrices(w, h, view, proj)) return;

    viewport.rt->beginRender(cmd);
    m_editor->renderSceneWithCamera(cmd, view, proj, w, h, useEditorExtras());
    onPostRender(cmd, w, h);
    viewport.rt->endRender(cmd);
}

void ViewportPanel::handleResize()
{
    if (!viewport.pendingResize) return;
    if (viewport.newWidth > 4 && viewport.newHeight > 4)
    {
        app->getD3D12()->flush();
        viewport.rt->resize(viewport.newWidth, viewport.newHeight);
        onResized(viewport.newWidth, viewport.newHeight);
    }
    viewport.pendingResize = false;
}

void ViewportPanel::drawContent()
{
    viewport.size = ImGui::GetContentRegionAvail();
    viewport.checkResize();

    if (viewport.isReady())
    {
        ImGui::Image((ImTextureID)viewport.rt->getSrvHandle().ptr, viewport.size);
        viewport.pos = ImGui::GetItemRectMin();
        onImageDrawn();
    }
    else textMuted("%s", notReadyText());

    onDrawOverlays();
}