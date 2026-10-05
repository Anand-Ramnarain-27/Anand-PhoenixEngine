#include "Globals.h"
#include "AshfallEnemyBuilder.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ResourceModel.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "HotReloadManager.h"
#include "PrefabManager.h"
#include "ComponentFactory.h"
#include "ComponentScript.h"
#include "ComponentAnimation.h"
#include "ComponentMesh.h"
#include <algorithm>
#include <functional>
#include <vector>

namespace {
    constexpr const char* kScriptClass = "EnemyBase";

    struct EnemyDef {
        const char* prefab;
        const char* config;         // Assets/Enemies/<config>
        const char* model;          // under Assets/
        const char* stateMachine;   // under Assets/
    };
    const EnemyDef kEnemies[] = {
        { "Goblin",         "goblin.json",          "Character Models/Goblin/GLTF Model/Goblin.gltf",           "StateMachines/Goblin.json" },
        { "Orc",            "orc.json",             "Character Models/Ork/GLTF Model/Orc.gltf",                 "StateMachines/Orc.json" },
        { "Orc_Drummer",    "orc_drummer.json",     "Character Models/Orc Drummer/GLTF Model/Orc_Drummer.gltf", "StateMachines/Orc_Drummer.json" },
        { "Orc_HyenaRider", "orc_hyena_rider.json", "Character Models/Orc Hyena/GLTF Model/Orc_hyena.gltf",     "StateMachines/Orc_hyena.json" },
    };

    // Same path the Asset Browser hands ModuleEditor::spawnModel; the "Assets/..." form is a fallback.
    UID modelUID(const std::string& assetsRoot, const std::string& relative){
        UID uid = app->getAssets()->findUID(assetsRoot + relative);
        return uid ? uid : app->getAssets()->findUID(std::string("Assets/") + relative);
    }

    void tagMeshNodes(GameObject* node, const std::string& tag){
        if (node->getComponent<ComponentMesh>()) node->setTag(tag);
        for (GameObject* c : node->getChildren()) tagMeshNodes(c, tag);
    }

    void destroySubtree(SceneGraph* scene, GameObject* root){
        // destroyGameObject only reparents a node's children, so collect the subtree and remove it bottom-up.
        std::vector<GameObject*> subtree;
        std::function<void(GameObject*)> collect = [&](GameObject* n){
            subtree.push_back(n);
            for (GameObject* c : n->getChildren()) collect(c);
        };
        collect(root);
        for (int i = (int)subtree.size() - 1; i >= 0; --i) scene->destroyGameObject(subtree[i]);
    }
}

bool BuildAshfallEnemyPrefabs(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage){
    if (!scene){ outMessage = "no active scene"; return false; }
    const std::vector<std::string> classes = hotReload ? hotReload->getRegisteredClassNames() : std::vector<std::string>();
    if (std::find(classes.begin(), classes.end(), kScriptClass) == classes.end()){
        outMessage = std::string("script class '") + kScriptClass + "' is not loaded: build GameScript.dll first";
        return false;
    }

    const std::string assets = app->getFileSystem()->GetAssetsPath();
    std::string saved;
    for (const EnemyDef& def : kEnemies){
        const UID uid = modelUID(assets, def.model);
        ResourceModel* model = uid ? app->getResources()->RequestModel(uid) : nullptr;
        if (!model){
            outMessage = std::string("can't load ") + def.model + " (is it imported?)" + (saved.empty() ? "" : "; saved so far: " + saved);
            return false;
        }
        GameObject* root = model->spawnIntoScene(scene);
        app->getResources()->ReleaseResource(model);
        if (!root){ outMessage = std::string("spawning ") + def.model + " failed"; return false; }

        root->setName(def.prefab);
        root->setTag("Enemy");
        if (auto* anim = root->getComponent<ComponentAnimation>())
            anim->LoadStateMachineFromPath(assets + def.stateMachine);   // starts its DefaultState (Idle)
        else
            LOG("[Enemies] %s has no ComponentAnimation: delete Library/Animations/<model>/ and re-import", def.model);
        for (GameObject* c : root->getChildren()) tagMeshNodes(c, "CameraIgnore");

        auto comp = ComponentFactory::CreateComponent(Component::Type::Script, root);
        comp->onLoad(std::string("{\"ClassName\":\"") + kScriptClass + "\",\"Fields\":{\"ConfigPath\":\"" + def.config + "\"}}");
        root->addComponent(std::move(comp));

        if (!PrefabManager::createPrefab(root, def.prefab)){
            outMessage = std::string("could not write the ") + def.prefab + " prefab: it is left in the scene";
            return false;
        }
        destroySubtree(scene, root);
        saved += (saved.empty() ? "" : ", ") + std::string(def.prefab);
    }
    outMessage = saved + " in " + app->getFileSystem()->GetLibraryPath() + "Prefabs/";
    return true;
}
