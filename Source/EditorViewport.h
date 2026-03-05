#pragma once
#include <imgui.h>
#include <memory>
#include <stdint.h>
#include "RenderTexture.h"

class ModuleD3D12;
class SceneManager;

struct EditorViewport {
    std::unique_ptr<RenderTexture> rt;
    ImVec2 size = {};
    ImVec2 pos = {};
    ImVec2 lastSize = {};
    bool pendingResize = false;
    uint32_t newWidth = 0;
    uint32_t newHeight = 0;

    bool isReady() const { return rt && rt->isValid() && size.x > 4 && size.y > 4; }

    void checkResize() {
        if (size.x > 4 && size.y > 4 && (size.x != lastSize.x || size.y != lastSize.y)) {
            pendingResize = true;
            newWidth = (uint32_t)size.x;
            newHeight = (uint32_t)size.y;
            lastSize = size;
        }
    }

    void applyResize(ModuleD3D12* d3d12, SceneManager* sm);
};