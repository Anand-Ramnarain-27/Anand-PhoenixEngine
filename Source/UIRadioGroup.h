#pragma once
#include "GameObject.h"
#include "SceneGraph.h"
#include "ComponentRadioGroup.h"
#include "ComponentCheckBox.h"
#include <cstdint>
#include <vector>

// Radio-group membership: tree-walk helpers, not a stored reference, so no cross-object link needs saving.
// Shared between ModuleUI (user clicks) and Phoenix_UI (script calls), which is why this lives in its own
// header rather than inline in either.
namespace UIRadioGroup {

// The nearest ancestor with a ComponentRadioGroup, or null. A checkbox's own group is the first one found
// walking up, so a group nested inside another only ever affects its own members.
inline GameObject* find(GameObject* go){
    for (GameObject* p = go ? go->getParent() : nullptr; p; p = p->getParent())
        if (p->getComponent<ComponentRadioGroup>()) return p;
    return nullptr;
}

namespace detail {
    inline void collectMemberUids(GameObject* node, std::vector<uint32_t>& out){
        if (!node) return;
        for (GameObject* child : node->getChildren()){
            if (child->getComponent<ComponentRadioGroup>()) continue;   // a nested group owns its own members
            if (child->getComponent<ComponentCheckBox>()) out.push_back(child->getUID());
            collectMemberUids(child, out);
        }
    }

    inline GameObject* findByUid(GameObject* node, uint32_t uid){
        if (!node) return nullptr;
        if (node->getUID() == uid) return node;
        for (GameObject* child : node->getChildren())
            if (GameObject* found = findByUid(child, uid)) return found;
        return nullptr;
    }
}

// Unchecks every other CheckBox belonging to `go`'s nearest ancestor group (if any), raising ValueChanged for
// each one that was actually on. Call this right after `go`'s own CheckBox has been turned on. Re-resolves each
// member by UID right before touching it, so an earlier listener destroying a later member is handled safely.
inline void applyExclusivity(SceneGraph* scene, GameObject* go){
    if (!scene || !go) return;
    GameObject* group = find(go);
    if (!group) return;

    std::vector<uint32_t> members;
    detail::collectMemberUids(group, members);

    const uint32_t skip = go->getUID();
    for (uint32_t uid : members){
        if (uid == skip) continue;
        GameObject* other = detail::findByUid(scene->getRoot(), uid);
        auto* box = other ? other->getComponent<ComponentCheckBox>() : nullptr;
        if (!box || !box->checked) continue;
        box->checked = false;
        box->onValueChanged.invoke(false);
    }
}

// The GameObject of the currently-checked member of `group` (the object carrying ComponentRadioGroup), or null
// if none is selected.
inline GameObject* findSelected(SceneGraph* scene, GameObject* group){
    if (!scene || !group) return nullptr;
    std::vector<uint32_t> members;
    detail::collectMemberUids(group, members);
    for (uint32_t uid : members){
        GameObject* member = detail::findByUid(scene->getRoot(), uid);
        auto* box = member ? member->getComponent<ComponentCheckBox>() : nullptr;
        if (box && box->checked) return member;
    }
    return nullptr;
}

} // namespace UIRadioGroup
