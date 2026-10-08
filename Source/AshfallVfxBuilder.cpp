#include "Globals.h"
#include "AshfallVfxBuilder.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleCamera.h"
#include "SceneManager.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentFactory.h"
#include "ComponentScript.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentMesh.h"
#include "HotReloadManager.h"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <vector>

namespace {
    constexpr const char* kRootName = "VFX_TestRoot";
    constexpr const char* kBenchScript = "VfxTestBench";
    // A 4 x 4 m dungeon floor tile (pivot on a corner, top at y = 0): the surface the levels' decals land on.
    // Procedural primitives aren't saved with the scene, so the floor must be a model.
    constexpr const char* kFloorTile = "Environment Models/Fantastic Dungeon Pack GLTF/Floor/PivotEdge/MOD_Floor_01_E_straight_med.gltf";
    constexpr float kTileSize = 4.f;
    constexpr int kTilesX = 10, kTilesZ = 8;   // 40 x 32 m

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

    template<class T> T* add(GameObject* go, Component::Type type){
        go->addComponent(ComponentFactory::CreateComponent(type, go));
        return go->getComponent<T>();
    }

    void place(GameObject* go, const Vector3& pos, const Quaternion& rot = Quaternion::Identity,
               const Vector3& scale = Vector3::One){
        ComponentTransform* t = go->getTransform();
        t->position = pos;
        t->rotation = rot;
        t->scale = scale;
        t->markDirty();
    }
}

bool BuildAshfallVfxTestScene(SceneManager* sm, HotReloadManager* hotReload, std::string& outMessage){
    SceneGraph* scene = sm ? sm->getModuleScene() : nullptr;
    if (!scene){ outMessage = "no active scene"; return false; }

    const std::vector<std::string> classes = hotReload ? hotReload->getRegisteredClassNames() : std::vector<std::string>();
    if (std::find(classes.begin(), classes.end(), kBenchScript) == classes.end()){
        outMessage = "script class 'VfxTestBench' is not loaded: build GameScript.dll first";
        return false;
    }

    // Never over a level: only a new (unsaved) scene, or VFX_Test itself.
    const std::string current = sm->getCurrentScenePath();
    if (!current.empty() && std::filesystem::path(current).stem().string() != kAshfallVfxTestScene){
        outMessage = "the open scene is " + std::filesystem::path(current).filename().string()
                   + " - use File > New Scene first (this action saves the scene as VFX_Test)";
        return false;
    }

    if (GameObject* old = scene->findGameObjectByName(kRootName)) destroySubtree(scene, old);
    GameObject* root = scene->createGameObject(kRootName);

    // 8 triggers per row, 4 m apart (VfxTestBench): ~28 m wide, ~24 m deep for the V1 set.
    {
        GameObject* floor = scene->createGameObject("VFX_Floor", root);
        const std::string tilePath = app->getFileSystem()->GetAssetsPath() + std::string(kFloorTile);
        int loaded = 0;
        for (int z = 0; z < kTilesZ; ++z)
            for (int x = 0; x < kTilesX; ++x){
                GameObject* tile = scene->createGameObject("VFX_FloorTile_" + std::to_string(z * kTilesX + x), floor);
                place(tile, Vector3((x - kTilesX * 0.5f) * kTileSize, 0.f, (z - kTilesZ * 0.5f) * kTileSize));
                if (auto* cm = add<ComponentMesh>(tile, Component::Type::Mesh); cm && cm->loadModel(tilePath.c_str())) ++loaded;
            }
        if (loaded == 0){
            outMessage = "floor tile model not found: " + tilePath;
            return false;
        }
    }

    // Dim, cool key: dark enough that glows, decals and the transient lights read the way they do in a dungeon.
    GameObject* sun = scene->createGameObject("VFX_Moon", root);
    if (auto* dl = add<ComponentDirectionalLight>(sun, Component::Type::DirectionalLight)){
        dl->direction = Vector3(0.3f, -1.f, 0.45f);
        dl->color = Vector3(0.62f, 0.68f, 0.85f);
        dl->intensity = 0.35f;
        dl->castShadows = false;
    }

    // Fixed camera above the near edge of the grid, looking down at its middle.
    GameObject* camGo = scene->createGameObject("VFX_Camera", root);
    const Vector3 eye(0.f, 20.f, -26.f), target(0.f, 0.f, 0.f);
    const Matrix view = Matrix::CreateLookAt(eye, target, Vector3::Up);
    place(camGo, eye, Quaternion::CreateFromRotationMatrix(view.Invert()));
    if (auto* cam = add<ComponentCamera>(camGo, Component::Type::Camera)) cam->setMainCamera(true);
    if (ModuleCamera* mc = app->getCamera()) mc->setActiveCamera(camGo);

    GameObject* bench = scene->createGameObject("VFX_TestBench", root);
    auto script = ComponentFactory::CreateComponent(Component::Type::Script, bench);
    script->onLoad(std::string("{\"ClassName\":\"") + kBenchScript + "\"}");
    bench->addComponent(std::move(script));

    const std::string path = app->getFileSystem()->GetLibraryPath() + "Scenes/" + kAshfallVfxTestScene + ".json";
    if (!sm->saveCurrentScene(path)){
        outMessage = "built, but saving " + path + " failed: save it by hand as VFX_Test";
        return false;
    }
    outMessage = path + " (press Play: the rows sweep every 6 s; Space fires every recipe, R reloads the recipes)";
    return true;
}
