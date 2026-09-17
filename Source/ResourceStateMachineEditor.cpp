// ResourceStateMachine::DrawInspector() - split out of ResourceStateMachine.cpp
// so that file (needed by ComponentAnimation::SendTrigger's FindState/FindClip
// lookups) can be linked into GameScript.dll without ImGui. Not virtual, so
// unlike ComponentAnimation's onEditor()/onDrawGizmos() there's no vtable/ctor
// entanglement here - this is a plain file split.
#include "Globals.h"
#include "ResourceStateMachine.h"

static constexpr ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

void ResourceStateMachine::DrawInspector(){

    if (ImGui::CollapsingHeader("Clips", ImGuiTreeNodeFlags_DefaultOpen)){
        int removeIdx = -1;
        if (ImGui::BeginTable("##smclips", 4, kTableFlags)){
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Animation UID", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 22.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)clips.size(); ++i){
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& clip = clips[i];

                ImGui::TableSetColumnIndex(0);
                char nameBuf[128] = {};
                strncpy_s(nameBuf, clip.name.str.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##n", nameBuf, sizeof(nameBuf)))
                    clip.name = std::string(nameBuf);

                ImGui::TableSetColumnIndex(1);
                char uidBuf[24] = {};
                sprintf_s(uidBuf, "%llu", static_cast<unsigned long long>(clip.animationUID));
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##u", uidBuf, sizeof(uidBuf), ImGuiInputTextFlags_CharsDecimal))
                    clip.animationUID = static_cast<UID>(strtoull(uidBuf, nullptr, 10));

                ImGui::TableSetColumnIndex(2);
                ImGui::Checkbox("##l", &clip.loop);

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("X")) removeIdx = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0) clips.erase(clips.begin() + removeIdx);
        if (ImGui::Button("+ Add Clip")) clips.push_back({});
    }

    if (ImGui::CollapsingHeader("States", ImGuiTreeNodeFlags_DefaultOpen)){
        std::vector<const char*> clipNames;
        clipNames.reserve(clips.size());
        for (const auto& c : clips) clipNames.push_back(c.name.str.c_str());

        int removeIdx = -1;
        if (ImGui::BeginTable("##smstates", 4, kTableFlags)){
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 22.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)states.size(); ++i){
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& state = states[i];

                ImGui::TableSetColumnIndex(0);
                char nameBuf[128] = {};
                strncpy_s(nameBuf, state.name.str.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##n", nameBuf, sizeof(nameBuf))){
                    bool wasDefault = (defaultState == state.name);
                    state.name = std::string(nameBuf);
                    if (wasDefault) defaultState = state.name;
                }

                ImGui::TableSetColumnIndex(1);
                {
                    int clipIdx = -1;
                    for (int j = 0; j < (int)clips.size(); ++j)
                        if (clips[j].name == state.clipName){ clipIdx = j; break; }
                    const char* preview = (clipIdx >= 0) ? clipNames[clipIdx] : "(none)";
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##c", preview)){
                        for (int j = 0; j < (int)clips.size(); ++j){
                            bool sel = (j == clipIdx);
                            if (ImGui::Selectable(clipNames[j], sel)) state.clipName = clips[j].name.str;
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::TableSetColumnIndex(2);
                {
                    bool isDefault = (defaultState == state.name);
                    if (ImGui::Checkbox("##d", &isDefault)){
                        if (isDefault) defaultState = state.name;
                        else if (defaultState == state.name) defaultState = HashString{};
                    }
                }

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("X")) removeIdx = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0){
            if (defaultState == states[removeIdx].name) defaultState = HashString{};
            states.erase(states.begin() + removeIdx);
        }
        if (ImGui::Button("+ Add State")) states.push_back({});
    }

    if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen)){
        std::vector<const char*> stateNames;
        stateNames.reserve(states.size());
        for (const auto& s : states) stateNames.push_back(s.name.str.c_str());

        auto drawStateCombo = [&](const char* id, HashString& field){
            int idx = -1;
            for (int j = 0; j < (int)states.size(); ++j)
                if (states[j].name == field){ idx = j; break; }
            const char* preview = (idx >= 0) ? stateNames[idx] : "(none)";
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo(id, preview)){
                for (int j = 0; j < (int)states.size(); ++j){
                    bool sel = (j == idx);
                    if (ImGui::Selectable(stateNames[j], sel)) field = states[j].name.str;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        };

        int removeIdx = -1;
        if (ImGui::BeginTable("##smtrans", 5, kTableFlags)){
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Trigger", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Blend ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 22.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)transitions.size(); ++i){
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& tr = transitions[i];

                ImGui::TableSetColumnIndex(0); drawStateCombo("##src", tr.source);
                ImGui::TableSetColumnIndex(1); drawStateCombo("##tgt", tr.target);

                ImGui::TableSetColumnIndex(2);
                char trigBuf[128] = {};
                strncpy_s(trigBuf, tr.trigger.str.c_str(), sizeof(trigBuf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##tr", trigBuf, sizeof(trigBuf)))
                    tr.trigger = std::string(trigBuf);

                ImGui::TableSetColumnIndex(3);
                int blendMs = static_cast<int>(tr.interpolationMs);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##b", &blendMs, 0, 2000))
                    tr.interpolationMs = static_cast<uint32_t>(blendMs);

                ImGui::TableSetColumnIndex(4);
                if (ImGui::SmallButton("X")) removeIdx = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0) transitions.erase(transitions.begin() + removeIdx);
        if (ImGui::Button("+ Add Transition")) transitions.push_back({});
    }
}
