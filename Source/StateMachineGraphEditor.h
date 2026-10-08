#pragma once
// Node-graph editor for animation state machines (imgui-node-editor).

#include <imgui.h>
#include <string>
#include "ResourceStateMachine.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

/// Draws a ResourceStateMachine as a graph: states are nodes, transitions are links. Dragging between pins adds a
/// transition, right-click menus edit or delete states and transitions, and the background menu adds a state.
class StateMachineGraphEditor {
public:
    StateMachineGraphEditor() = default;
    ~StateMachineGraphEditor(){ Shutdown(); }

    void Init(const std::string& settingsFilePath);
    void Shutdown();

    /// Draws and edits `sm` in place; `activeState` (when playing) is highlighted.
    void Draw(ResourceStateMachine& sm, const HashString* activeState = nullptr);

private:
    void drawNodes(const ResourceStateMachine& sm, const HashString* activeState);
    void drawLinks(const ResourceStateMachine& sm);
    void handleCreate(ResourceStateMachine& sm);
    void handleDelete(ResourceStateMachine& sm);
    void queryContextMenus(const ResourceStateMachine& sm);
    void drawBackgroundPopup(ResourceStateMachine& sm);
    void drawNodePopup(ResourceStateMachine& sm);
    void drawLinkPopup(ResourceStateMachine& sm);

    ax::NodeEditor::EditorContext* m_context = nullptr;
    std::string m_settingsFile;

    int m_contextNodeIdx = -1;
    int m_contextLinkIdx = -1;
    bool m_showNodeMenu = false;
    bool m_showLinkMenu = false;
    bool m_showBgMenu = false;
    ImVec2 m_newNodeCanvasPos = {};

    int m_pendingNodeIdx = -1;
    ImVec2 m_pendingNodePos = {};

    char m_nodeNameBuf[128] = {};
    char m_nodeClipBuf[128] = {};
    char m_linkTriggerBuf[128] = {};
};
