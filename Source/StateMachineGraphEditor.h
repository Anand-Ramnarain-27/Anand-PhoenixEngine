#pragma once
#include <imgui.h>
#include <string>
#include "ResourceStateMachine.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

class StateMachineGraphEditor {
public:
    StateMachineGraphEditor() = default;
    ~StateMachineGraphEditor() { Shutdown(); }

    void Init(const std::string& settingsFilePath);
    void Shutdown();

    // Draws the full immediate-mode node canvas and all context menus.
    // Call once per frame while the owning window is open.
    // Pass activeState to highlight the currently-playing state node in green.
    void Draw(ResourceStateMachine& sm, const HashString* activeState = nullptr);

private:
    ax::NodeEditor::EditorContext* m_context = nullptr;
    std::string m_settingsFile;

    // Context menu trigger state (set inside Begin/End, consumed in Suspend block)
    int m_contextNodeIdx = -1;
    int m_contextLinkIdx = -1;
    bool m_showNodeMenu = false;
    bool m_showLinkMenu = false;
    bool m_showBgMenu = false;
    ImVec2 m_newNodeCanvasPos = {};

    // Deferred node position for newly-created states
    int m_pendingNodeIdx = -1;
    ImVec2 m_pendingNodePos = {};

    // In-popup edit buffers (populated when popup opens)
    char m_nodeNameBuf[128] = {};
    char m_nodeClipBuf[128] = {};
    char m_linkTriggerBuf[128] = {};
};
