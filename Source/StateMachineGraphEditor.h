#pragma once
#include <imgui.h>
#include <string>
#include "ResourceStateMachine.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

class StateMachineGraphEditor {
public:
    StateMachineGraphEditor() = default;
    ~StateMachineGraphEditor(){ Shutdown(); }

    void Init(const std::string& settingsFilePath);
    void Shutdown();

    void Draw(ResourceStateMachine& sm, const HashString* activeState = nullptr);

private:
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
