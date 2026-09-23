#include "Globals.h"
#include "ComponentRadioGroup.h"
#include "UIJson.h"
#include <imgui.h>

void ComponentRadioGroup::onEditor(){
    ImGui::Checkbox("Allow Switch Off##radio", &allowSwitchOff);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Off (default): the group always keeps one option selected.\nOn: clicking the selected option can leave nothing chosen.");
    ImGui::TextDisabled("Every CheckBox under this object is a member,\nunless it is under a nested Radio Group.");
}

void ComponentRadioGroup::onSave(std::string& outJson) const{
    UIJson::putBool(outJson, "allowSwitchOff", allowSwitchOff);
}

void ComponentRadioGroup::onLoad(const std::string& json){
    UIJson::Reader r(json);
    r.getBool("allowSwitchOff", allowSwitchOff);
}
