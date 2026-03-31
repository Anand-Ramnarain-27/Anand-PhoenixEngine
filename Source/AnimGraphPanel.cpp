#include "Globals.h"
#include "AnimGraphPanel.h"
#include "ComponentAnimation.h"
#include "ModuleEditor.h"
#include <imgui.h>
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

AnimGraphPanel::~AnimGraphPanel() {
    if (m_context) { ed::DestroyEditor(m_context); m_context = nullptr; }
}

void AnimGraphPanel::setTarget(ComponentAnimation* comp) {
    if (m_target == comp) return;
    if (m_context) { ed::DestroyEditor(m_context); m_context = nullptr; }
    m_target = comp;
    m_resource = comp ? comp->getStateMachineResource() : nullptr;
    if (!m_resource) return;
    ed::Config cfg;
    static char settingsPath[256];
    snprintf(settingsPath, sizeof(settingsPath),
        "AnimGraph_%p.json", (void*)comp);
    cfg.SettingsFile = settingsPath;
    m_context = ed::CreateEditor(&cfg);
}

void AnimGraphPanel::drawContent() {
    if (!m_resource || !m_context) {
        ImGui::TextDisabled("No ComponentAnimation selected");
        return;
    }
    ed::SetCurrentEditor(m_context);
    ed::Begin("AnimGraph");
    drawNodes();
    drawLinks();
    handleCreation();
    handleDeletion();
    handleContextMenus();
    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void AnimGraphPanel::drawNodes() {
    for (int i = 0; i < (int)m_resource->states.size(); ++i) {
        SMState& st = m_resource->states[i];
        int nodeId = i * 10 + 1;
        int pinInId = i * 10 + 2;
        int pinOutId = i * 10 + 3;

        bool isDefault = (st.name == m_resource->defaultState);
        if (isDefault)
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(1.f, 0.85f, 0.f, 1.f));

        ed::BeginNode(nodeId);
        ImGui::TextColored(isDefault ? ImVec4(1, 0.85f, 0, 1) : ImVec4(1, 1, 1, 1),
            "%s", st.name.c_str());
        ImGui::Separator();
        ImGui::Text("  Clip: %s", st.clipName.c_str());
        if (isDefault) ImGui::Text("  Default");

        ed::BeginPin(pinInId, ed::PinKind::Input);
        ImGui::Text("In");
        ed::EndPin();
        ImGui::SameLine(80.0f);
        ed::BeginPin(pinOutId, ed::PinKind::Output);
        ImGui::Text("Out");
        ed::EndPin();

        ed::EndNode();
        if (isDefault) ed::PopStyleColor();
    }
}

void AnimGraphPanel::drawLinks() {
    for (int i = 0; i < (int)m_resource->transitions.size(); ++i) {
        const SMTransition& tr = m_resource->transitions[i];
        int linkId = i * 10 + 4;
        int srcIdx = -1, dstIdx = -1;
        for (int j = 0; j < (int)m_resource->states.size(); ++j) {
            if (m_resource->states[j].name == tr.source) srcIdx = j;
            if (m_resource->states[j].name == tr.target) dstIdx = j;
        }
        if (srcIdx < 0 || dstIdx < 0) continue;
        int srcPin = srcIdx * 10 + 3; 
        int dstPin = dstIdx * 10 + 2; 
        ed::Link(linkId, srcPin, dstPin);
    }
}

void AnimGraphPanel::handleCreation() {
    if (ed::BeginCreate()) {

        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {

            int rawStart = (int)startPinId.Get();
            int rawEnd = (int)endPinId.Get();
            bool startIsOut = (rawStart % 10 == 3);
            bool endIsIn = (rawEnd % 10 == 2);
            if (!startIsOut || !endIsIn) {
                ed::RejectNewItem(ImVec4(1, 0, 0, 1), 2.0f);
            }
            else if (ed::AcceptNewItem()) {
    
                SMTransition tr;
                tr.source = m_resource->states[nodeIdxFromOutPin(rawStart)].name;
                tr.target = m_resource->states[nodeIdxFromPinId(rawEnd)].name;
                tr.triggerName = "NewTrigger";
                tr.interpolationMs = 200.0f;
                m_resource->transitions.push_back(tr);
            }
        }

        ed::PinId newNodePinId;                     
        if (ed::QueryNewNode(&newNodePinId)) {
            if (ed::AcceptNewItem()) {
                SMState st;
                st.name = "State" + std::to_string(m_resource->states.size());

                auto mousePos = ed::ScreenToCanvas(ImGui::GetMousePos());
                st.nodeX = mousePos.x;
                st.nodeY = mousePos.y;
                m_resource->states.push_back(st);
                if (m_resource->defaultState.empty())
                    m_resource->defaultState = st.name;
            }
        }
    }
    ed::EndCreate();
}

void AnimGraphPanel::handleDeletion() {
    if (ed::BeginDelete()) {
        ed::NodeId delNodeId;
        while (ed::QueryDeletedNode(&delNodeId)) {
            if (ed::AcceptDeletedItem()) {
                int idx = nodeIdxFromNodeId((int)delNodeId.Get());
                if (idx >= 0 && idx < (int)m_resource->states.size())
                    m_resource->states.erase(m_resource->states.begin() + idx);
            }
        }
        ed::LinkId delLinkId;
        while (ed::QueryDeletedLink(&delLinkId)) {
            if (ed::AcceptDeletedItem()) {
                int idx = linkIdxFromLinkId((int)delLinkId.Get());
                if (idx >= 0 && idx < (int)m_resource->transitions.size())
                    m_resource->transitions.erase(
                        m_resource->transitions.begin() + idx);
            }
        }
    }
    ed::EndDelete();
}

void AnimGraphPanel::handleContextMenus() {
    ed::NodeId ctxNodeId;
    ed::LinkId ctxLinkId;

    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("AnimGraphBG");
    }
    if (ImGui::BeginPopup("AnimGraphBG")) {
        if (ImGui::MenuItem("Add State")) {
            SMState st;
            st.name = "State" + std::to_string(m_resource->states.size());
            m_resource->states.push_back(st);
            if (m_resource->defaultState.empty())
                m_resource->defaultState = st.name;
        }
        ImGui::EndPopup();
    }

    if (ed::ShowNodeContextMenu(&ctxNodeId))
        ImGui::OpenPopup("AnimGraphNode");
    if (ImGui::BeginPopup("AnimGraphNode")) {
        int idx = nodeIdxFromNodeId((int)ctxNodeId.Get());
        if (idx >= 0 && idx < (int)m_resource->states.size()) {
            SMState& st = m_resource->states[idx];
     
            static char nameBuf[64] = {};
            strncpy_s(nameBuf, st.name.c_str(), sizeof(nameBuf) - 1);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                st.name = nameBuf;
   
            if (ImGui::BeginCombo("Clip", st.clipName.c_str())) {
                for (const auto& cl : m_resource->clips) {
                    bool sel = (cl.name == st.clipName);
                    if (ImGui::Selectable(cl.name.c_str(), sel))
                        st.clipName = cl.name;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            bool isDefault = (st.name == m_resource->defaultState);
            if (ImGui::Checkbox("Default", &isDefault) && isDefault)
                m_resource->defaultState = st.name;
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                m_resource->states.erase(m_resource->states.begin() + idx);
            }
        }
        ImGui::EndPopup();
    }

    if (ed::ShowLinkContextMenu(&ctxLinkId))
        ImGui::OpenPopup("AnimGraphLink");
    if (ImGui::BeginPopup("AnimGraphLink")) {
        int idx = linkIdxFromLinkId((int)ctxLinkId.Get());
        if (idx >= 0 && idx < (int)m_resource->transitions.size()) {
            SMTransition& tr = m_resource->transitions[idx];
            static char trigBuf[64] = {};
            strncpy_s(trigBuf, tr.triggerName.c_str(), sizeof(trigBuf) - 1);
            if (ImGui::InputText("Trigger", trigBuf, sizeof(trigBuf)))
                tr.triggerName = trigBuf;
            ImGui::InputFloat("Blend (ms)", &tr.interpolationMs, 10.0f, 100.0f);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                m_resource->transitions.erase(
                    m_resource->transitions.begin() + idx);
            }
        }
        ImGui::EndPopup();
    }
}
