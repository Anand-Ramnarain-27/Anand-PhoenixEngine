#include "Globals.h"
#include "StateMachineGraphEditor.h"
#include "ResourceStateMachine.h"
#include <imgui_node_editor.h>
#include <algorithm>

namespace ed = ax::NodeEditor;


static int stateIdxFromNodeId(uintptr_t raw){ return (int)(raw - 1) / 10; }
static int stateIdxFromInPin (uintptr_t raw){ return (int)(raw - 2) / 10; }
static int stateIdxFromOutPin(uintptr_t raw){ return (int)(raw - 3) / 10; }
static int transIdxFromLinkId(uintptr_t raw){ return (int)raw - 1000000; }

static constexpr ImVec4 kYellow { 1.f, 1.f, 0.f, 1.f };
static constexpr ImVec4 kRed { 1.f, 0.f, 0.f, 1.f };


void StateMachineGraphEditor::Init(const std::string& settingsFilePath){
    m_settingsFile = settingsFilePath;
    ed::Config cfg;
    cfg.SettingsFile = m_settingsFile.c_str();
    m_context = ed::CreateEditor(&cfg);
}

void StateMachineGraphEditor::Shutdown(){
    if (m_context){
        ed::DestroyEditor(m_context);
        m_context = nullptr;
    }
}


void StateMachineGraphEditor::Draw(ResourceStateMachine& sm, const HashString* activeState){
    if (!m_context) return;

    ed::SetCurrentEditor(m_context);
    ed::Begin("##AnimGraph");

    if (m_pendingNodeIdx >= 0 && m_pendingNodeIdx < (int)sm.states.size()){
        ed::SetNodePosition(ed::NodeId(m_pendingNodeIdx * 10 + 1), m_pendingNodePos);
        m_pendingNodeIdx = -1;
    }

    for (int i = 0; i < (int)sm.states.size(); ++i){
        const auto& st = sm.states[i];
        bool isDef = (st.name == sm.defaultState);

        ed::BeginNode(ed::NodeId(i * 10 + 1));
        ImGui::PushID(i);

        ed::BeginPin(ed::PinId(i * 10 + 2), ed::PinKind::Input);
        ImGui::Text(">");
        ed::EndPin();

        ImGui::SameLine();

        const bool isActive = activeState && (st.name == *activeState);
        static constexpr ImVec4 kGreen { 0.2f, 1.f, 0.4f, 1.f };
        ImGui::BeginGroup();
        if (isActive)
            ImGui::TextColored(kGreen, "%s", st.name.str.c_str());
        else if (isDef)
            ImGui::TextColored(kYellow, "%s", st.name.str.c_str());
        else
            ImGui::Text("%s", st.name.str.c_str());
        if (!st.clipName.empty())
            ImGui::TextDisabled("[%s]", st.clipName.str.c_str());
        if (isActive)
            ImGui::TextColored(kGreen, "ACTIVE");
        else if (isDef)
            ImGui::TextColored(kYellow, "Default");
        ImGui::EndGroup();

        ImGui::SameLine();

        ed::BeginPin(ed::PinId(i * 10 + 3), ed::PinKind::Output);
        ImGui::Text(">");
        ed::EndPin();

        ImGui::PopID();
        ed::EndNode();
    }

    for (int i = 0; i < (int)sm.transitions.size(); ++i){
        const auto& tr = sm.transitions[i];
        int si = sm.FindStateIndex(tr.source);
        int di = sm.FindStateIndex(tr.target);
        if (si < 0 || di < 0) continue;
        ed::Link(ed::LinkId(1000000 + i),
                 ed::PinId(si * 10 + 3),
                 ed::PinId(di * 10 + 2));
    }

    if (ed::BeginCreate()){
        ed::PinId startPin, endPin;
        if (ed::QueryNewLink(&startPin, &endPin)){
            uintptr_t s = startPin.Get(), e = endPin.Get();

            if (s % 10 == 2){ std::swap(s, e); std::swap(startPin, endPin); }

            if (s % 10 != 3 || e % 10 != 2){
                ed::RejectNewItem(kRed, 2.f);
            } else {
                int si = stateIdxFromOutPin(s);
                int di = stateIdxFromInPin(e);
                bool valid = si >= 0 && si < (int)sm.states.size()
                          && di >= 0 && di < (int)sm.states.size()
                          && si != di;
                if (!valid){
                    ed::RejectNewItem(kRed, 2.f);
                } else if (ed::AcceptNewItem()){
                    SMTransition t;
                    t.source = sm.states[si].name;
                    t.target = sm.states[di].name;
                    t.trigger = HashString(std::string("NewTrigger"));
                    t.interpolationMs = 300;
                    sm.transitions.push_back(std::move(t));
                }
            }
        }
        ed::EndCreate();
    }

    if (ed::BeginDelete()){
        ed::LinkId delLink;
        std::vector<int> linksToErase;
        while (ed::QueryDeletedLink(&delLink)){
            int idx = transIdxFromLinkId(delLink.Get());
            if (idx >= 0 && idx < (int)sm.transitions.size()){
                if (ed::AcceptDeletedItem()) linksToErase.push_back(idx);
            } else {
                ed::RejectDeletedItem();
            }
        }
        std::sort(linksToErase.rbegin(), linksToErase.rend());
        for (int idx : linksToErase)
            sm.transitions.erase(sm.transitions.begin() + idx);

        ed::NodeId delNode;
        while (ed::QueryDeletedNode(&delNode)){
            int idx = stateIdxFromNodeId(delNode.Get());
            if (idx >= 0 && idx < (int)sm.states.size()){
                if (ed::AcceptDeletedItem(false)){
                    const HashString name = sm.states[idx].name;
                    sm.transitions.erase(
                        std::remove_if(sm.transitions.begin(), sm.transitions.end(),
                            [&](const SMTransition& t){
                                return t.source == name || t.target == name;
                            }),
                        sm.transitions.end());
                    if (sm.defaultState == name) sm.defaultState = HashString{};
                    sm.states.erase(sm.states.begin() + idx);
                }
            }
        }
        ed::EndDelete();
    }

    {
        ed::NodeId ctxNode;
        ed::LinkId ctxLink;

        if (ed::ShowNodeContextMenu(&ctxNode)){
            int idx = stateIdxFromNodeId(ctxNode.Get());
            if (idx >= 0 && idx < (int)sm.states.size()){
                m_contextNodeIdx = idx;
                strncpy_s(m_nodeNameBuf, sm.states[idx].name.str.c_str(), sizeof(m_nodeNameBuf) - 1);
                strncpy_s(m_nodeClipBuf, sm.states[idx].clipName.str.c_str(), sizeof(m_nodeClipBuf) - 1);
                m_showNodeMenu = true;
            }
        }
        if (ed::ShowLinkContextMenu(&ctxLink)){
            int idx = transIdxFromLinkId(ctxLink.Get());
            if (idx >= 0 && idx < (int)sm.transitions.size()){
                m_contextLinkIdx = idx;
                strncpy_s(m_linkTriggerBuf, sm.transitions[idx].trigger.str.c_str(), sizeof(m_linkTriggerBuf) - 1);
                m_showLinkMenu = true;
            }
        }
        if (ed::ShowBackgroundContextMenu()){
            m_newNodeCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
            m_showBgMenu = true;
        }
    }

    ed::End();

    ed::Suspend();

    if (m_showNodeMenu){ ImGui::OpenPopup("##NodeCtx"); m_showNodeMenu = false; }
    if (m_showLinkMenu){ ImGui::OpenPopup("##LinkCtx"); m_showLinkMenu = false; }
    if (m_showBgMenu){ ImGui::OpenPopup("##BgCtx"); m_showBgMenu = false; }

    if (ImGui::BeginPopup("##BgCtx")){
        if (ImGui::MenuItem("New State")){
            SMState s;
            s.name = HashString(std::string("NewState"));
            sm.states.push_back(s);
            m_pendingNodeIdx = (int)sm.states.size() - 1;
            m_pendingNodePos = m_newNodeCanvasPos;
        }
        ImGui::EndPopup();
    }

    if (m_contextNodeIdx >= 0 && m_contextNodeIdx < (int)sm.states.size()){
        if (ImGui::BeginPopup("##NodeCtx")){
            SMState& st = sm.states[m_contextNodeIdx];

            ImGui::TextDisabled("State");
            ImGui::Separator();

            ImGui::Text("Name");
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::InputText("##ename", m_nodeNameBuf, sizeof(m_nodeNameBuf))){
                bool wasDef = (sm.defaultState == st.name);
                for (auto& t : sm.transitions){
                    if (t.source == st.name) t.source = std::string(m_nodeNameBuf);
                    if (t.target == st.name) t.target = std::string(m_nodeNameBuf);
                }
                st.name = std::string(m_nodeNameBuf);
                if (wasDef) sm.defaultState = st.name;
            }

            ImGui::Text("Clip");
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::InputText("##eclip", m_nodeClipBuf, sizeof(m_nodeClipBuf)))
                st.clipName = std::string(m_nodeClipBuf);

            bool isDef = (sm.defaultState == st.name);
            if (ImGui::Checkbox("Default", &isDef)){
                if (isDef) sm.defaultState = st.name;
                else if (sm.defaultState == st.name) sm.defaultState = HashString{};
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete")){
                const HashString name = st.name;
                sm.transitions.erase(
                    std::remove_if(sm.transitions.begin(), sm.transitions.end(),
                        [&](const SMTransition& t){ return t.source == name || t.target == name; }),
                    sm.transitions.end());
                if (sm.defaultState == name) sm.defaultState = HashString{};
                sm.states.erase(sm.states.begin() + m_contextNodeIdx);
                m_contextNodeIdx = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    if (m_contextLinkIdx >= 0 && m_contextLinkIdx < (int)sm.transitions.size()){
        if (ImGui::BeginPopup("##LinkCtx")){
            SMTransition& tr = sm.transitions[m_contextLinkIdx];

            ImGui::TextDisabled("Transition");
            ImGui::Separator();

            ImGui::Text("Trigger");
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::InputText("##etrig", m_linkTriggerBuf, sizeof(m_linkTriggerBuf)))
                tr.trigger = std::string(m_linkTriggerBuf);

            int blendMs = (int)tr.interpolationMs;
            ImGui::Text("Blend ms");
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::SliderInt("##eblend", &blendMs, 0, 2000))
                tr.interpolationMs = (uint32_t)blendMs;

            ImGui::Separator();
            if (ImGui::MenuItem("Delete")){
                sm.transitions.erase(sm.transitions.begin() + m_contextLinkIdx);
                m_contextLinkIdx = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    ed::Resume();
    ed::SetCurrentEditor(nullptr);
}
