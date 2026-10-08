#pragma once
// Render targets of one editor viewport (Scene or Game view), resized with its window.

#include <imgui.h>
#include <memory>
#include <stdint.h>
#include "RenderTexture.h"

/// HDR scene target, LDR display target, their ping-pong scratch copies and the bloom mips. checkResize()
/// notes a new window size; ViewportPanel::handleResize() then resizes the targets.
struct EditorViewport {
    static constexpr int kNumBloomMips = 3;

    std::unique_ptr<RenderTexture> rt;
    std::unique_ptr<RenderTexture> rtScratch;
    std::unique_ptr<RenderTexture> display;
    std::unique_ptr<RenderTexture> displayScratch;
    std::unique_ptr<RenderTexture> bloomMips[kNumBloomMips];
    ImVec2 size = {};
    ImVec2 pos = {};
    ImVec2 lastSize = {};
    bool pendingResize = false;
    uint32_t newWidth = 0;
    uint32_t newHeight = 0;

    bool isReady() const { return rt && rt->isValid() && display && display->isValid() && size.x > 4 && size.y > 4; }

    void checkResize(){
        if (size.x > 4 && size.y > 4 && (size.x != lastSize.x || size.y != lastSize.y)){
            pendingResize = true;
            newWidth = (uint32_t)size.x;
            newHeight = (uint32_t)size.y;
            lastSize = size;
        }
    }
};
