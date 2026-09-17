// ComponentAnimation::SendTrigger()/pushLayer() - split out of
// ComponentAnimation.cpp so they can be linked into GameScript.dll (via
// PhoenixCore) without the rest of that file: onEditor()/onDrawGizmos()
// (ImGui + debug-draw gizmos), update()/applyAnimation() (ComponentMesh
// morph targets), and the constructor (which - if in the same .obj as these
// two methods - would pin the whole vtable, forcing onEditor/onDrawGizmos to
// resolve regardless of whether they're ever called). GameScript.dll never
// constructs a ComponentAnimation itself, only calls SendTrigger() on one
// that already exists, so the ctor doesn't need to be here.
//
// This is the one real per-frame effect: pushLayer() calls
// ModuleResources::RequestAnimation(), which is why ModuleResources.cpp
// (trimmed - see ModuleResourcesFactories.cpp/ModuleResourcesFactoriesStub.cpp)
// and ResourceAnimation.cpp are also in PhoenixCore now.
#include "Globals.h"
#include "ComponentAnimation.h"
#include "ModuleResources.h"
#include "ResourceAnimation.h"
#include "Application.h"

void ComponentAnimation::pushLayer(UID animUID, float transitionTimeMs, bool loop){
    ResourceAnimation* anim = app->getResources()->RequestAnimation(animUID);
    if (!anim){
        LOG("ComponentAnimation::pushLayer: failed to load animation uid=%llu", animUID);
        return;
    }
    AnimLayer* layer = new AnimLayer();
    layer->anim = anim;
    layer->currentTimeMs = 0.f;
    layer->fadeTimeMs = 0.f;
    layer->transitionTimeMs = transitionTimeMs;
    layer->loop = loop;
    layer->next = m_layerHead;
    m_layerHead = layer;
}

void ComponentAnimation::SendTrigger(const HashString& trigger){
    if (!m_stateMachine) return;

    for (const auto& tr : m_stateMachine->transitions){
        if (tr.source != m_activeState || tr.trigger != trigger) continue;

        const SMState* target = m_stateMachine->FindState(tr.target);
        if (!target) return;

        const SMClip* clip = m_stateMachine->FindClip(target->clipName);
        if (!clip || clip->animationUID == 0) return;

        pushLayer(clip->animationUID, (float)tr.interpolationMs, clip->loop);
        m_activeState = tr.target;
        return;
    }
}
