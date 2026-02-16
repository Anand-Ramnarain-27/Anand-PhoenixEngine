#include "Globals.h"
#include "RenderPipelineTestScene.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ModuleCamera.h"
#include "DebugDrawPass.h"
#include "ModuleScene.h"
#include "Application.h"
#include "ResourceCache.h"

RenderPipelineTestScene::RenderPipelineTestScene() = default;
RenderPipelineTestScene::~RenderPipelineTestScene() = default;

const char* RenderPipelineTestScene::getName() const
{
    return "Render Pipeline Test";
}

const char* RenderPipelineTestScene::getDescription() const
{
    return "Scene with multiple models to test asset pipeline.";
}

//bool RenderPipelineTestScene::initialize(ID3D12Device*)
//{
//    scene = std::make_unique<ModuleScene>();
//
//    parent = scene->createGameObject("Parent");
//    child = scene->createGameObject("Child", parent);
//
//    parent->getTransform()->position = { 0, 0, 0 };
//    child->getTransform()->position = { 0, 1, 0 };
//
//    duckObject = scene->createGameObject("Duck", parent);
//    duckObject->getTransform()->position = { -2, 0, 0 };
//    duckObject->getTransform()->scale = { 0.01f, 0.01f, 0.01f };
//
//    ComponentMesh* duckMesh = duckObject->createComponent<ComponentMesh>();
//    if (!duckMesh->loadModel("Assets/Models/Duck/duck.gltf"))
//    {
//        LOG("Failed to load duck model!");
//    }
//    else
//    {
//        LOG("Duck model loaded successfully");
//    }
//
//    secondModel = scene->createGameObject("Duck", parent);
//    secondModel->getTransform()->position = { 2, 0, 0 };
//    secondModel->getTransform()->scale = { 0.01f, 0.01f, 0.01f };
//
//    ComponentMesh* houseMesh = secondModel->createComponent<ComponentMesh>();
//    if (!houseMesh->loadModel("Assets/Models/Duck/duck.gltf"))
//    {
//        LOG("Failed to load Duck model - file might not exist");
//        LOG("That's okay, just using duck for now");
//    }
//    else
//    {
//        LOG("Duck model loaded successfully");
//    }
//
//    m_time = 0.0f;
//    return true;
//}

bool RenderPipelineTestScene::initialize(ID3D12Device*)
{
    scene = std::make_unique<ModuleScene>();

    // Create parent/child as before
    parent = scene->createGameObject("Parent");
    child = scene->createGameObject("Child", parent);

    // ? ADD THIS: Log cache stats BEFORE loading
    ResourceCache* cache = app->getResourceCache();
    int models1, meshes1, materials1;
    cache->getStats(models1, meshes1, materials1);
    LOG("?? BEFORE loading - Cached models: %d", models1);

    // Load first duck
    duckObject = scene->createGameObject("Duck", parent);
    duckObject->getTransform()->position = { -2, 0, 0 };
    duckObject->getTransform()->scale = { 0.01f, 0.01f, 0.01f };
    ComponentMesh* duckMesh = duckObject->createComponent<ComponentMesh>();
    duckMesh->loadModel("Assets/Models/Duck/duck.gltf");

    // ? ADD THIS: Check cache after first load
    int models2, meshes2, materials2;
    cache->getStats(models2, meshes2, materials2);
    LOG("?? AFTER first duck - Cached models: %d", models2);

    // Load second duck (same model!)
    secondModel = scene->createGameObject("Duck2", parent);
    secondModel->getTransform()->position = { 2, 0, 0 };
    secondModel->getTransform()->scale = { 0.01f, 0.01f, 0.01f };
    ComponentMesh* houseMesh = secondModel->createComponent<ComponentMesh>();
    houseMesh->loadModel("Assets/Models/Duck/duck.gltf");

    // ? ADD THIS: Check cache after second load
    int models3, meshes3, materials3;
    cache->getStats(models3, meshes3, materials3);
    LOG("?? AFTER second duck - Cached models: %d (should still be 1!)", models3);

    m_time = 0.0f;
    return true;
}

void RenderPipelineTestScene::update(float deltaTime)
{
    m_time += deltaTime;

    parent->getTransform()->rotation = Quaternion::CreateFromAxisAngle(Vector3::Up, m_time * 0.5f);

    parent->getTransform()->markDirty();

    scene->update(deltaTime);
}

void RenderPipelineTestScene::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera&, uint32_t, uint32_t)
{
    dd::xzSquareGrid(-5.0f, 5.0f, 0.0f, 1.0f, dd::colors::LightGray);

    dd::axisTriad(ddConvert(parent->getTransform()->getGlobalMatrix()), 0.3f, 1.0f);
    dd::axisTriad(ddConvert(child->getTransform()->getGlobalMatrix()), 0.2f, 1.0f);

    if (duckObject)
    {
        dd::axisTriad(ddConvert(duckObject->getTransform()->getGlobalMatrix()), 0.1f, 1.0f);
    }

    if (secondModel)
    {
        dd::axisTriad(ddConvert(secondModel->getTransform()->getGlobalMatrix()), 0.1f, 1.0f);
    }

    scene->getRoot()->render(cmd);
}

void RenderPipelineTestScene::shutdown()
{
    scene.reset();
}