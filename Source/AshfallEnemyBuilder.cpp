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
#include "SceneManager.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <filesystem>
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
        float scale = 1.f;          // root scale (Kraug: a big Troll) [TUNE]
    };
    const EnemyDef kEnemies[] = {
        { "Goblin",         "goblin.json",          "Character Models/Goblin/GLTF Model/Goblin.gltf",           "StateMachines/Goblin.json" },
        { "Orc",            "orc.json",             "Character Models/Ork/GLTF Model/Orc.gltf",                 "StateMachines/Orc.json" },
        { "Orc_Drummer",    "orc_drummer.json",     "Character Models/Orc Drummer/GLTF Model/Orc_Drummer.gltf", "StateMachines/Orc_Drummer.json" },
        { "Orc_HyenaRider", "orc_hyena_rider.json", "Character Models/Orc Hyena/GLTF Model/Orc_hyena.gltf",     "StateMachines/Orc_hyena.json" },
        { "Kraug",          "kraug.json",           "Character Models/Troll/GLTF Model/Troll.gltf",             "StateMachines/Troll.json", 1.35f },
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
        if (def.scale != 1.f){ root->getTransform()->scale = Vector3(def.scale, def.scale, def.scale); root->getTransform()->markDirty(); }
        if (auto* anim = root->getComponent<ComponentAnimation>())
            anim->LoadStateMachineFromPath(assets + def.stateMachine);   // starts its DefaultState (Idle)
        else
            PHX_LOG(Editor, Warning, "[Enemies] %s has no ComponentAnimation: delete Library/Animations/<model>/ and re-import", def.model);
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

// ---------------------------------------------------------------- Set Up Kraug Arena

namespace {
    std::string hierarchyPath(GameObject* go){
        std::string p;
        for (GameObject* n = go; n && n->getParent(); n = n->getParent()) p = n->getName() + (p.empty() ? "" : "/" + p);
        return p;
    }

    ComponentScript* scriptOf(GameObject* go, const std::string& cls){
        for (const auto& c : go->getComponents())
            if (c->getType() == Component::Type::Script && static_cast<ComponentScript*>(c.get())->getClassName() == cls)
                return static_cast<ComponentScript*>(c.get());
        return nullptr;
    }

    // Rewrites some of a script component's saved fields (its own onSave / onLoad round trip).
    void setFields(ComponentScript* cs, const std::function<void(rapidjson::Value&, rapidjson::Document::AllocatorType&)>& edit){
        std::string json;
        cs->onSave(json);
        rapidjson::Document d;
        d.Parse(json.c_str());
        if (d.HasParseError() || !d.IsObject()) return;
        auto& a = d.GetAllocator();
        if (!d.HasMember("Fields") || !d["Fields"].IsObject()) d.AddMember("Fields", rapidjson::Value(rapidjson::kObjectType), a);
        edit(d["Fields"], a);
        rapidjson::StringBuffer buf;
        rapidjson::Writer<rapidjson::StringBuffer> w(buf);
        d.Accept(w);
        cs->onLoad(buf.GetString());
    }

    void setMember(rapidjson::Value& obj, const char* key, rapidjson::Value v, rapidjson::Document::AllocatorType& a){
        auto it = obj.FindMember(key);
        if (it != obj.MemberEnd()) it->value = v;
        else obj.AddMember(rapidjson::Value(key, a), v, a);
    }

    GameObject* outermostPrefabRoot(GameObject* go){
        GameObject* root = nullptr;
        for (GameObject* n = go; n; n = n->getParent()) if (PrefabManager::isPrefabInstance(n)) root = n;
        return root;
    }
}

bool SetUpAshfallKraugArena(SceneManager* sm, HotReloadManager* hotReload, std::string& outMessage){
    SceneGraph* scene = sm ? sm->getModuleScene() : nullptr;
    if (!scene){ outMessage = "no active scene"; return false; }
    const std::string current = sm->getCurrentScenePath();
    if (std::filesystem::path(current).stem().string() != "AF_KraugsDen"){
        outMessage = "open AF_KraugsDen first (the open scene is " + (current.empty() ? std::string("unsaved") : current) + ")";
        return false;
    }
    const std::vector<std::string> classes = hotReload ? hotReload->getRegisteredClassNames() : std::vector<std::string>();
    if (std::find(classes.begin(), classes.end(), "BreakablePillar") == classes.end()){
        outMessage = "script class 'BreakablePillar' is not loaded: build GameScript.dll first";
        return false;
    }

    std::vector<GameObject*> touched;
    std::string done;
    struct PillarWiring { const char* pillar; const char* crack; const char* light; };
    const PillarWiring pillars[] = {
        { "T5_CrackedPillar_W", "PH_T5_CrackDecal_W", "T5_CrackRim_1" },   // all three at x +7.14 (the layout's west, mirrored)
        { "T5_CrackedPillar_E", "PH_T5_CrackDecal_E", "T5_CrackRim_2" },   // x -7.14
    };
    for (const PillarWiring& w : pillars){
        GameObject* go = scene->findGameObjectByName(w.pillar);
        if (!go){ outMessage = std::string(w.pillar) + " not found in the scene"; return false; }
        ComponentScript* cs = scriptOf(go, "BreakablePillar");
        if (!cs){
            auto comp = ComponentFactory::CreateComponent(Component::Type::Script, go);
            comp->onLoad("{\"ClassName\":\"BreakablePillar\"}");
            cs = static_cast<ComponentScript*>(comp.get());
            go->addComponent(std::move(comp));
        }
        GameObject* crack = scene->findGameObjectByName(w.crack);
        GameObject* light = scene->findGameObjectByName(w.light);
        setFields(cs, [&](rapidjson::Value& f, rapidjson::Document::AllocatorType& a){
            auto ref = [&](GameObject* target){
                rapidjson::Value v(rapidjson::kObjectType);
                const std::string p = target ? hierarchyPath(target) : std::string();
                v.AddMember("path", rapidjson::Value(p.c_str(), a), a);
                return v;
            };
            setMember(f, "CrackMarker", ref(crack), a);
            setMember(f, "CrackLight", ref(light), a);
        });
        touched.push_back(go);
        done += std::string(done.empty() ? "" : ", ") + w.pillar;
    }

    if (GameObject* exit = scene->findGameObjectByName("Exit_DenBack")){
        if (ComponentScript* cs = scriptOf(exit, "LevelExit")){
            setFields(cs, [&](rapidjson::Value& f, rapidjson::Document::AllocatorType& a){
                setMember(f, "RequiresCleared", rapidjson::Value("AF_KraugsDen", a), a);
                setMember(f, "MarksCleared", rapidjson::Value(false), a);
            });
            touched.push_back(exit);
            done += ", Exit_DenBack (requires the den cleared)";
        }
    }
    else done += " (no Exit_DenBack found)";

    // The editor's Apply for each prefab instance these objects live in, so a fresh instance has them too.
    std::vector<GameObject*> applied;
    for (GameObject* go : touched){
        GameObject* root = outermostPrefabRoot(go);
        if (!root || std::find(applied.begin(), applied.end(), root) != applied.end()) continue;
        if (PrefabManager::applyToPrefab(root)) applied.push_back(root);
    }
    for (GameObject* r : applied) done += ", applied to prefab " + PrefabManager::getPrefabName(r);

    if (!sm->saveCurrentScene(current)){ outMessage = "wired (" + done + ") but saving " + current + " failed: save it by hand"; return false; }
    outMessage = done + "; saved " + current;
    return true;
}
