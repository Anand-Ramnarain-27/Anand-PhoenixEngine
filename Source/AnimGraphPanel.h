#pragma once
#include "EditorPanel.h"
#include "StateMachineResource.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

class ComponentAnimation;

class AnimGraphPanel : public EditorPanel {
public:
    explicit AnimGraphPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    ~AnimGraphPanel() override;

    const char* getName() const override { return "Anim Graph"; }

    void setTarget(ComponentAnimation* comp);

protected:
    void drawContent() override;

private:
    void drawNodes();
    void drawLinks();
    void handleCreation();
    void handleDeletion();
    void handleContextMenus();

    ax::NodeEditor::EditorContext* m_context = nullptr;
    ComponentAnimation* m_target = nullptr;
    StateMachineResource* m_resource = nullptr;

    int m_newLinkSrcPin = -1;
    int m_newLinkDstPin = -1;

    int m_ctxNodeIdx = -1;
    int m_ctxLinkIdx = -1;

    static int nodeIdxFromNodeId(int id) { return (id - 1) / 10; }
    static int nodeIdxFromPinId(int id) { return (id - 2) / 10; } 
    static int nodeIdxFromOutPin(int id) { return (id - 3) / 10; }  
    static int linkIdxFromLinkId(int id) { return (id - 4) / 10; }
};
