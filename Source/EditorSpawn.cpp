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

namespace fs = std::filesystem;

GameObject* ModuleEditor::createEmptyGameObject(const char* name, GameObject* parent){
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return nullptr;
    GameObject* go = scene->createGameObject(name);
    if (parent) go->setParent(parent);
    m_selection.object = go;
    log(("Created: " + std::string(name)).c_str(), EditorColors::Success);

    std::string goName = go->getName();
    auto serialized = std::make_shared<std::string>();
    auto livePtr = std::make_shared<GameObject*>(go);

    pushCommand({
        [this, serialized, livePtr, goName](){
            SceneGraph* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = serialized->empty()
                ? s->createGameObject(goName.c_str())
                : PrefabManager::deserializeGameObject(*serialized, s);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo create: " + goName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, serialized, goName](){
            GameObject* go = *livePtr;
            if (!go) return;
            *serialized = PrefabManager::serializeGameObject(go);
            if (m_selection.object == go) m_selection.clear();
            app->getD3D12()->flush();
            SceneGraph* s = getActiveModuleScene();
            if (s) s->destroyGameObject(go);
            *livePtr = nullptr;
            log(("Undo create: " + goName).c_str(), EditorColors::Warning);
        }
        });

    return go;
}

void ModuleEditor::deleteGameObject(GameObject* go){
    if (!go) return;
    if (m_selection.object == go || isChildOf(go, m_selection.object)) m_selection.clear();
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return;

    std::vector<GameObject*> subtree;
    {
        std::function<void(GameObject*)> collect = [&](GameObject* node){
            subtree.push_back(node);
            for (auto* c : node->getChildren()) collect(c);
        };
        collect(go);
    }

    if (ModuleCamera* cam = app->getCamera()){
        GameObject* active = cam->getActiveCamera();
        if (active && (active == go || isChildOf(go, active))){
            cam->setActiveCamera(nullptr);
            cam->clearGameCameraFrustum();
        }
    }

    {
        std::function<void(GameObject*)> scanMeshes = [&](GameObject* node){
            if (!node) return;
            if (auto* cm = node->getComponent<ComponentMesh>()){
                for (GameObject* doomed : subtree) cm->nullifyJoint(doomed);
            }
            for (auto* c : node->getChildren()) scanMeshes(c);
        };
        scanMeshes(scene->getRoot());
    }

    std::string name = go->getName();

    app->getD3D12()->flush();
    for (int i = (int)subtree.size() - 1; i >= 0; --i)
        scene->destroyGameObject(subtree[i]);

    log(("Deleted: " + name).c_str(), EditorColors::Warning);

    pushCommand({
        [](){},
        [](){}
    });
}

void ModuleEditor::spawnAssetAtPath(const std::string& path){
    if (path.empty() || !fs::exists(path) || fs::is_directory(path)) return;
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    SceneGraph* scene = getActiveModuleScene();
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj"){
        if (!scene) return;
        if (GameObject* go = spawnModel(path)) m_selection.object = go;
    }
    else if (ext == ".json"){
        if (m_sceneManager && m_sceneManager->loadScene(path)) log(("Loaded scene: " + path).c_str(), EditorColors::Success);
    }
    else if (ext == ".prefab"){
        if (!scene) return;
        std::string stem = fs::path(path).stem().string();
        PrefabManager::instantiatePrefab(stem, scene);
        log(("Instantiated: " + stem).c_str(), EditorColors::Success);
    }
}

GameObject* ModuleEditor::spawnModel(const std::string& path){
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    std::string stem = fs::path(path).stem().string();

    UID uid = app->getAssets()->findUID(path);
    if (uid == 0){
        log(("Cannot find UID for: " + path).c_str(), EditorColors::Danger);
        return nullptr;
    }

    ResourceModel* model = app->getResources()->RequestModel(uid);
    if (model){
        int meshNodeCount = 0;
        for (const auto& n : model->getNodes())
            if (!n.meshes.empty()) ++meshNodeCount;

        bool hasAnimations = (app->getAssets()->findSubUID(path, "anim", 0) != 0);
        bool hasSkin = !model->getSkins().empty();
        bool needsHierarchy = meshNodeCount > 1 || hasSkin || hasAnimations;
        if (needsHierarchy){
            GameObject* root = model->spawnIntoScene(scene);
            app->getResources()->ReleaseResource(model);
            if (root){
                bool animComp = root->getComponent<ComponentAnimation>() != nullptr;
                LOG("spawnModel '%s': meshNodes=%d skin=%s anim=%s AnimComponent=%s",
                    stem.c_str(), meshNodeCount,
                    hasSkin ? "yes" : "no",
                    hasAnimations ? "yes" : "no",
                    animComp ? "yes" : "no");
                log(("Added: " + stem).c_str(), EditorColors::Success);
                return root;
            }
        } else {
            app->getResources()->ReleaseResource(model);
        }
    }

    GameObject* go = scene->createGameObject(stem);
    bool ok = go->createComponent<ComponentMesh>()->loadModel(path.c_str());
    log(ok ? ("Added: " + stem).c_str() : ("Failed: " + path).c_str(),
        ok ? EditorColors::Success : EditorColors::Danger);
    return ok ? go : nullptr;
}

GameObject* ModuleEditor::spawnPrimitive(PrimitiveType type,
                                          const Vector3& position,
                                          const Vector3& scale,
                                          bool addPhysics){
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    static const char* kNames[] = { "Cube","Sphere","Capsule","Plane","Cylinder" };
    int ti = static_cast<int>(type);
    const char* name = (ti >= 0 && ti < 5) ? kNames[ti] : "Primitive";

    std::unique_ptr<Mesh> mesh;
    switch (type){
    case PrimitiveType::Cube: mesh = PrimitiveFactory::createCubeMesh(); break;
    case PrimitiveType::Sphere: mesh = PrimitiveFactory::createSphereMesh(); break;
    case PrimitiveType::Capsule: mesh = PrimitiveFactory::createCapsuleMesh(); break;
    case PrimitiveType::Plane: mesh = PrimitiveFactory::createPlaneMesh(); break;
    case PrimitiveType::Cylinder: mesh = PrimitiveFactory::createCylinderMesh(); break;
    default: return nullptr;
    }

    GameObject* go = scene->createGameObject(name);
    auto* t = go->getTransform();
    t->position = position;
    t->scale = scale;
    t->markDirty();

    auto* cm = go->createComponent<ComponentMesh>();
    cm->setProceduralModel(PrimitiveFactory::meshToModel(std::move(mesh)));

    if (type == PrimitiveType::Sphere){
        auto* cb = go->createComponent<ComponentBounds>();
        cb->bvType = BVType::Sphere;
    }

    if (addPhysics){
        auto* rb = go->createComponent<ComponentRigidbody>();
        rb->mass = 1.f;
        rb->useGravity = true;
        rb->restitution = 0.5f;
        rb->linearDamping = 0.1f;
    }

    m_selection.object = go;
    log(std::string("Spawned ").append(name).c_str(), EditorColors::Success);
    return go;
}

GameObject* ModuleEditor::spawnFireParticleSystem(const Vector3& position){
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    const std::string fireTex = "Library/Textures/TXT_Fire_01.dds";
    const std::string sparksTex = "Library/Textures/TXT_Sparks_01.dds";

    GameObject* root = scene->createGameObject("Fire (Exercise 1)");
    root->getTransform()->position = position;
    root->getTransform()->markDirty();

    {
        GameObject* go = scene->createGameObject("Flames", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.35f;
        ps->coneAngleDeg = 18.f;
        ps->emissionRate = 30.f;
        ps->maxParticles = 200;
        ps->lifeRange = Vector2(0.8f, 1.4f);
        ps->speedRange = Vector2(0.8f, 1.6f);
        ps->sizeRange = Vector2(0.4f, 0.8f);
        ps->rotationRange = Vector2(-45.f, 45.f);
        ps->startColor = Vector4(1.f, 0.95f, 0.6f, 1.f);
        ps->endColor = Vector4(1.f, 0.25f, 0.05f, 0.f);
        ps->startSizeMul = 0.6f;
        ps->endSizeMul = 1.4f;
        ps->texturePath = fireTex;
        ps->sheetColumns = 2;
        ps->sheetRows = 2;
        ps->randomFrame = true;
        ps->blendMode = ComponentParticleSystem::BlendMode::Alpha;
        ps->layer = 0;
    }

    {
        GameObject* go = scene->createGameObject("Flames Inner Light", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.2f;
        ps->coneAngleDeg = 14.f;
        ps->emissionRate = 24.f;
        ps->maxParticles = 150;
        ps->lifeRange = Vector2(0.6f, 1.0f);
        ps->speedRange = Vector2(0.8f, 1.5f);
        ps->sizeRange = Vector2(0.18f, 0.35f);
        ps->rotationRange = Vector2(-45.f, 45.f);
        ps->startColor = Vector4(1.f, 1.f, 0.85f, 1.f);
        ps->endColor = Vector4(1.f, 0.5f, 0.1f, 0.f);
        ps->startSizeMul = 0.7f;
        ps->endSizeMul = 1.2f;
        ps->texturePath = fireTex;
        ps->sheetColumns = 2;
        ps->sheetRows = 2;
        ps->randomFrame = true;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
        ps->layer = 1;
    }

    {
        GameObject* go = scene->createGameObject("Fire Glow", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.4f;
        ps->coneAngleDeg = 10.f;
        ps->emissionRate = 8.f;
        ps->maxParticles = 60;
        ps->lifeRange = Vector2(1.2f, 2.0f);
        ps->speedRange = Vector2(0.2f, 0.5f);
        ps->sizeRange = Vector2(1.0f, 1.6f);
        ps->rotationRange = Vector2(-30.f, 30.f);
        ps->startColor = Vector4(1.f, 0.6f, 0.2f, 0.35f);
        ps->endColor = Vector4(1.f, 0.3f, 0.05f, 0.f);
        ps->startSizeMul = 0.5f;
        ps->endSizeMul = 1.8f;
        ps->texturePath = fireTex;
        ps->sheetColumns = 1;
        ps->sheetRows = 1;
        ps->randomFrame = false;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
        ps->layer = 2;
    }

    {
        GameObject* go = scene->createGameObject("Sparks", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.15f;
        ps->coneAngleDeg = 30.f;
        ps->emissionRate = 15.f;
        ps->maxParticles = 100;
        ps->lifeRange = Vector2(0.4f, 0.9f);
        ps->speedRange = Vector2(2.0f, 4.5f);
        ps->sizeRange = Vector2(0.04f, 0.10f);
        ps->rotationRange = Vector2(0.f, 0.f);
        ps->startColor = Vector4(1.f, 0.95f, 0.6f, 1.f);
        ps->endColor = Vector4(1.f, 0.35f, 0.05f, 0.f);
        ps->startSizeMul = 1.f;
        ps->endSizeMul = 0.3f;
        ps->gravity = Vector3(0.f, -1.5f, 0.f);
        ps->texturePath = sparksTex;
        ps->sheetColumns = 1;
        ps->sheetRows = 1;
        ps->randomFrame = false;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
        ps->layer = 3;

        ps->useTurbulence = true;
        ps->turbulenceFrequency = 0.6f;
        ps->turbulenceStrength = 1.2f;
        ps->turbulenceOctaves = 3;
        ps->turbulenceScroll = 0.4f;
    }

    m_selection.object = root;
    log("Spawned Fire (Exercise 1): using TXT_Fire_01 + TXT_Sparks_01 textures", EditorColors::Success);
    return root;
}

GameObject* ModuleEditor::spawnSwordTrail(const Vector3& position){
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    const std::string swordModel = "Assets/Models/Sword/sword.gltf";
    const std::string swooshTex = "Assets/Models/Sword/swoosh.png";

    GameObject* root = scene->createGameObject("Sword Trail");
    {
        auto* t = root->getTransform();
        t->position = position;
        t->markDirty();
    }

    {
        GameObject* swordGO = scene->createGameObject("Sword", root);
        auto* t = swordGO->getTransform();
        t->position = Vector3(0.f, 0.f, 0.f);
        t->scale = Vector3(1.f, 1.f, 1.f);
        t->markDirty();

        auto* cm = swordGO->createComponent<ComponentMesh>();
        if (!cm->loadModel(swordModel.c_str()))
            LOG("spawnSwordTrail: could not load '%s'", swordModel.c_str());
    }

    {
        auto* tr = root->createComponent<ComponentTrail>();
        tr->enabled = true;
        tr->emitting = true;
        tr->duration = 0.45f;
        tr->minPointDistance = 0.02f;
        tr->width = 0.55f;

        tr->useCatmullRom = true;
        tr->catmullRomAlpha = 0.5f;
        tr->subdivisions = 10;

        tr->startColor = Vector4(1.f, 1.f, 0.85f, 0.95f);
        tr->endColor = Vector4(1.f, 0.55f, 0.1f, 0.0f);
        tr->startWidthMul = 1.0f;
        tr->endWidthMul = 0.05f;

        tr->texturePath = swooshTex;
        tr->blendMode = ComponentTrail::BlendMode::Additive;
        tr->textureMode = ComponentTrail::TextureMode::Stretch;
        tr->layer = 0;
    }

    m_selection.object = root;
    log("Spawned Sword Trail: sword.gltf + swoosh.png", EditorColors::Success);
    return root;
}

GameObject* ModuleEditor::spawnFireComet(const Vector3& position){
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    const std::string fireTex = "Library/Textures/TXT_Fire_01.dds";
    const std::string sparksTex = "Library/Textures/TXT_Sparks_01.dds";

    GameObject* root = scene->createGameObject("Fire Comet");
    {
        auto* t = root->getTransform();
        t->position = position;
        t->markDirty();
    }

    {
        auto* tr = root->createComponent<ComponentTrail>();
        tr->enabled = true;
        tr->emitting = true;
        tr->duration = 1.2f;
        tr->minPointDistance = 0.015f;
        tr->width = 0.8f;
        tr->maxSegmentAngle = 15.f;

        tr->useCatmullRom = true;
        tr->catmullRomAlpha = 0.5f;
        tr->subdivisions = 10;

        tr->startColor = Vector4(0.30f, 0.70f, 1.0f, 1.0f);
        tr->endColor = Vector4(0.10f, 1.0f, 0.60f, 0.0f);
        tr->startWidthMul = 2.0f;
        tr->endWidthMul = 0.0f;

        tr->texturePath = "";
        tr->blendMode = ComponentTrail::BlendMode::Additive;
        tr->textureMode = ComponentTrail::TextureMode::Stretch;
        tr->layer = 0;

        tr->previewOrbit = true;
        tr->orbitRadius = 1.5f;
        tr->orbitSpeed = 2.5f;
    }

    {
        GameObject* flameGO = scene->createGameObject("Core Flames", root);
        flameGO->getTransform()->markDirty();

        auto* ps = flameGO->createComponent<ComponentParticleSystem>();
        ps->enabled = true;
        ps->playing = true;
        ps->looping = true;
        ps->emissionRate = 75.f;
        ps->maxParticles = 160;

        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.06f;
        ps->coneAngleDeg = 18.f;
        ps->worldSpace = true;

        ps->lifeRange = Vector2(0.20f, 0.45f);
        ps->speedRange = Vector2(0.5f, 1.4f);
        ps->sizeRange = Vector2(0.13f, 0.22f);
        ps->rotationRange = Vector2(-30.f, 30.f);

        ps->startColor = Vector4(1.00f, 0.85f, 0.30f, 1.00f);
        ps->endColor = Vector4(0.90f, 0.12f, 0.00f, 0.00f);
        ps->startSizeMul = 1.0f;
        ps->endSizeMul = 0.05f;

        ps->gravity = Vector3(0.f, 0.35f, 0.f);
        ps->useTurbulence = false;

        ps->texturePath = fireTex;
        ps->sheetColumns = 2;
        ps->sheetRows = 2;
        ps->randomFrame = true;
        ps->blendMode = ComponentParticleSystem::BlendMode::Alpha;
    }

    {
        GameObject* emberGO = scene->createGameObject("Embers", root);
        emberGO->getTransform()->markDirty();

        auto* ps = emberGO->createComponent<ComponentParticleSystem>();
        ps->enabled = true;
        ps->playing = true;
        ps->looping = true;
        ps->emissionRate = 32.f;
        ps->maxParticles = 80;

        ps->shape = ComponentParticleSystem::EmitterShape::Sphere;
        ps->shapeRadius = 0.10f;
        ps->worldSpace = true;

        ps->lifeRange = Vector2(0.55f, 1.20f);
        ps->speedRange = Vector2(0.25f, 0.75f);
        ps->sizeRange = Vector2(0.035f, 0.065f);
        ps->rotationRange = Vector2(-180.f, 180.f);

        ps->startColor = Vector4(1.00f, 0.72f, 0.10f, 1.00f);
        ps->endColor = Vector4(0.85f, 0.10f, 0.00f, 0.00f);
        ps->startSizeMul = 1.0f;
        ps->endSizeMul = 0.0f;

        ps->gravity = Vector3(0.f, -0.40f, 0.f);
        ps->useTurbulence = true;
        ps->turbulenceFrequency = 1.5f;
        ps->turbulenceStrength = 1.2f;
        ps->turbulenceOctaves = 3;
        ps->turbulenceScroll = 0.5f;

        ps->texturePath = sparksTex;
        ps->sheetColumns = 1;
        ps->sheetRows = 1;
        ps->randomFrame = false;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
    }

    if (PrefabManager::createPrefab(root, "FireComet"))
        log("FireComet prefab saved — instantiate any time from Asset Browser", EditorColors::Success);

    m_effectsPlaying = true;
    m_effectsTime = 0.f;

    m_selection.object = root;
    log("Spawned Fire Comet: orbiting in edit mode — trail + flames + embers live (disable Preview Orbit in Inspector before Play)", EditorColors::Success);
    return root;
}
