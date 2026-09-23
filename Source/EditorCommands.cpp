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
#include <vector>
#include <functional>
#include <unordered_set>
#include <cfloat>

namespace {
    GameObject* findByUid(GameObject* node, uint32_t uid){
        if (!node) return nullptr;
        if (node->getUID() == uid) return node;
        for (GameObject* child : node->getChildren())
            if (GameObject* found = findByUid(child, uid)) return found;
        return nullptr;
    }
}

void ModuleEditor::pushCommand(EditorCommand cmd){
    m_redoStack.clear();
    m_undoStack.push_back(std::move(cmd));
    if ((int)m_undoStack.size() > kMaxUndoSteps){
        m_undoStack.pop_front();
        if (m_savePointIndex > 0) --m_savePointIndex;
    }
}

bool ModuleEditor::canUndo() const { return (int)m_undoStack.size() > m_savePointIndex; }

bool ModuleEditor::canRedo() const { return !m_redoStack.empty(); }

void ModuleEditor::undoToSavePoint(){
    if (!canUndo()) return;
    EditorCommand& cmd = m_undoStack.back();
    cmd.undo();
    m_redoStack.push_back(std::move(cmd));
    m_undoStack.pop_back();
}

void ModuleEditor::redo(){
    if (!canRedo()) return;
    EditorCommand& cmd = m_redoStack.back();
    cmd.execute();
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.pop_back();
}

void ModuleEditor::copySelected(){
    if (!m_selection.has()) return;
    m_clipboard.name = m_selection.object->getName();
    m_clipboard.serialized = PrefabManager::serializeGameObject(m_selection.object);
    log(("Copied: " + m_clipboard.name).c_str(), EditorColors::Info);
}

void ModuleEditor::pasteClipboard(){
    if (m_clipboard.serialized.empty()) return;
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return;
    GameObject* pasted = PrefabManager::deserializeGameObject(m_clipboard.serialized, scene);
    if (!pasted) return;
    std::string pastedName = pasted->getName();
    m_selection.object = pasted;
    log(("Pasted: " + pastedName).c_str(), EditorColors::Success);

    std::string clipData = m_clipboard.serialized;
    auto livePtr = std::make_shared<GameObject*>(pasted);

    pushCommand({
        [this, clipData, livePtr, pastedName](){
            SceneGraph* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(clipData, s);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo paste: " + pastedName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, pastedName](){
            GameObject* p = *livePtr;
            if (!p) return;
            if (m_selection.object == p) m_selection.clear();
            app->getD3D12()->flush();
            SceneGraph* s = getActiveModuleScene();
            if (s) s->destroyGameObject(p);
            *livePtr = nullptr;
            log(("Undo paste: " + pastedName).c_str(), EditorColors::Warning);
        }
        });
}

void ModuleEditor::pushCreateSubtreeUndo(GameObject* root, const char* label){
    if (!root) return;
    std::string name = label ? label : root->getName();
    auto serialized = std::make_shared<std::string>();
    auto livePtr = std::make_shared<GameObject*>(root);
    // Serializing captures the subtree's own contents, not where it sits in the tree, so the parent is tracked
    // separately by UID and re-resolved on redo (falling back to the scene root if it is somehow gone by then).
    const uint32_t parentUid = root->getParent() ? root->getParent()->getUID() : 0;

    pushCommand({
        [this, serialized, livePtr, name, parentUid](){
            SceneGraph* s = getActiveModuleScene();
            if (!s || serialized->empty()) return;
            GameObject* parent = parentUid ? findByUid(s->getRoot(), parentUid) : nullptr;
            GameObject* restored = PrefabManager::deserializeGameObject(*serialized, s, parent);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo create: " + name).c_str(), EditorColors::Success); }
        },
        [this, livePtr, serialized, name](){
            GameObject* go = *livePtr;
            if (!go) return;
            *serialized = PrefabManager::serializeGameObject(go);
            if (m_selection.object == go || isChildOf(go, m_selection.object)) m_selection.clear();
            app->getD3D12()->flush();
            SceneGraph* s = getActiveModuleScene();
            if (s){
                // SceneGraph::destroyGameObject only reparents a node's direct children rather than removing
                // them, so a single call on the widget's root would silently orphan its own child widgets
                // (a Button's Image+Label, a Checkbox's Text, a Radio Group's options). Collect the whole
                // subtree first and destroy it bottom-up instead, same as ModuleEditor::deleteGameObject.
                std::vector<GameObject*> subtree;
                std::function<void(GameObject*)> collect = [&](GameObject* node){
                    subtree.push_back(node);
                    for (auto* c : node->getChildren()) collect(c);
                };
                collect(go);
                for (int i = (int)subtree.size() - 1; i >= 0; --i) s->destroyGameObject(subtree[i]);
            }
            *livePtr = nullptr;
            log(("Undo create: " + name).c_str(), EditorColors::Warning);
        }
        });
}

void ModuleEditor::duplicateSelected(){
    if (!m_selection.has()) return;
    std::string serialized = PrefabManager::serializeGameObject(m_selection.object);
    std::string srcName = m_selection.object->getName();
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return;
    GameObject* dupe = PrefabManager::deserializeGameObject(serialized, scene);
    if (!dupe) return;
    std::string dupeName = dupe->getName();
    m_selection.object = dupe;
    log(("Duplicated: " + dupeName).c_str(), EditorColors::Success);

    auto livePtr = std::make_shared<GameObject*>(dupe);

    pushCommand({
        [this, serialized, livePtr, dupeName](){
            SceneGraph* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(serialized, s);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo duplicate: " + dupeName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, dupeName](){
            GameObject* d = *livePtr;
            if (!d) return;
            if (m_selection.object == d) m_selection.clear();
            app->getD3D12()->flush();
            SceneGraph* s = getActiveModuleScene();
            if (s) s->destroyGameObject(d);
            *livePtr = nullptr;
            log(("Undo duplicate: " + dupeName).c_str(), EditorColors::Warning);
        }
        });
}
