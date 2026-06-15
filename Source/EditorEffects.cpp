#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include <ole2.h>
#include "DragDropManager.h"
#include "EngineDropTarget.h"
#include "ModuleD3D12.h"
#include "ComponentAnimation.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ImGuiPass.h"
#include "DebugDrawPass.h"
#include "ForwardMeshPass.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "DecalPass.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
#include "RenderTexture.h"
#include "EmptyScene.h"
#include "SceneGraph.h"
#include "SceneManager.h"
#include "ShaderTableDesc.h"
#include "MeshPipeline.h"
#include "FileDialog.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentFactory.h"
#include "PrimitiveFactory.h"
#include "ModuleCamera.h"
#include "FrustumDebugDraw.h"
#include "BoundingVolume.h"
#include "ComponentBounds.h"
#include "ComponentRigidbody.h"
#include "CollisionSystem.h"
#include "CollisionResponse.h"
#include "EnvironmentMap.h"
#include "SceneViewPanel.h"
#include "GameViewPanel.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "AssetBrowserPanel.h"
#include "SceneSettingsPanel.h"
#include "PrefabManager.h"
#include "FileWatcher.h"
#include "ResourceMaterial.h"
#include "ResourceModel.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "Model.h"
#include "Mesh.h"
#include "MeshEntry.h"
#include "ResourceMesh.h"
#include <d3dx12.h>
#include "ModuleStaticBuffer.h"
#include <filesystem>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <cfloat>

template<typename TrailFn, typename PsFn>
static void forEachEffect(GameObject* root, TrailFn trailFn, PsFn psFn){
    if (!root) return;
    std::function<void(GameObject*)> visit = [&](GameObject* go){
        if (!go || !go->isActive()) return;
        if (auto* tr = go->getComponent<ComponentTrail>()) trailFn(tr);
        if (auto* ps = go->getComponent<ComponentParticleSystem>()) psFn(ps);
        for (auto* c : go->getChildren()) visit(c);
    };
    visit(root);
}

void ModuleEditor::effectsStop(){
    m_effectsPlaying = false;
    m_effectsTime = 0.f;
    SceneGraph* ms = getActiveModuleScene();
    if (!ms) return;
    forEachEffect(ms->getRoot(),
        [](ComponentTrail* tr){ tr->clear(); },
        [](ComponentParticleSystem* ps){ ps->clear(); });
}

void ModuleEditor::effectsRestartAll(){
    SceneGraph* ms = getActiveModuleScene();
    if (!ms) return;
    forEachEffect(ms->getRoot(),
        [](ComponentTrail* tr){ tr->clear(); },
        [](ComponentParticleSystem* ps){ ps->clear(); });
    m_effectsPlaying = true;
    m_effectsTime = 0.f;
    log("Effects restarted (all)", EditorColors::Info);
}

void ModuleEditor::effectsRestartSelected(){
    if (m_selection.has()){
        bool did = false;
        if (auto* tr = m_selection.object->getComponent<ComponentTrail>())
            { tr->clear(); did = true; }
        if (auto* ps = m_selection.object->getComponent<ComponentParticleSystem>())
            { ps->clear(); did = true; }
        if (did){
            m_effectsPlaying = true;
            m_effectsTime = 0.f;
            log("Effects restarted (selected)", EditorColors::Info);
            return;
        }
    }
    effectsRestartAll();
}

void ModuleEditor::updateEffectsInEditMode(float dt){
    SceneGraph* ms = getActiveModuleScene();
    if (!ms) return;
    m_effectsTime += dt;
    std::function<void(GameObject*)> visit = [&](GameObject* go){
        if (!go || !go->isActive()) return;
        if (auto* tr = go->getComponent<ComponentTrail>()) tr->update(dt);
        if (auto* ps = go->getComponent<ComponentParticleSystem>()) ps->update(dt);
        for (auto* c : go->getChildren()) visit(c);
    };
    visit(ms->getRoot());
}
