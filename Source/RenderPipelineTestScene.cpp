#include "Globals.h"
#include "RenderPipelineTestScene.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ModuleCamera.h"
#include "DebugDrawPass.h"
#include "ModuleScene.h"
#include "Model.h"

RenderPipelineTestScene::RenderPipelineTestScene() = default;
RenderPipelineTestScene::~RenderPipelineTestScene() = default;

const char* RenderPipelineTestScene::getName() const
{
    return "Render Pipeline Test";
}

const char* RenderPipelineTestScene::getDescription() const
{
    return "Minimal scene to validate scene lifecycle and viewport rendering.";
}

bool RenderPipelineTestScene::initialize(ID3D12Device*)
{
    scene = std::make_unique<ModuleScene>();

    parent = scene->createGameObject("Parent");
    child = scene->createGameObject("Child", parent);

    parent->getTransform()->position = { 0, 0, 0 };
    child->getTransform()->position = { 0, 1, 0 };

    // CREATE A DUCK GAME OBJECT
    GameObject* duckObject = scene->createGameObject("Duck", parent);
    duckObject->getTransform()->position = { 0, 0, 0 };
    duckObject->getTransform()->scale = { 0.01f, 0.01f, 0.01f };  // Scale it down

    // ADD MESH COMPONENT
    ComponentMesh* meshComp = duckObject->createComponent<ComponentMesh>();
    if (!meshComp->loadModel("Assets/Models/Duck/duck.gltf"))
    {
        LOG("Failed to load duck model!");
    }
    else
    {
        LOG("Duck model loaded successfully");
    }

    m_time = 0.0f;
    return true;
}

void RenderPipelineTestScene::update(float deltaTime)
{
    m_time += deltaTime;

    parent->getTransform()->rotation = Quaternion::CreateFromAxisAngle(Vector3::Up, m_time);

    parent->getTransform()->markDirty();

    scene->update(deltaTime);
}

void RenderPipelineTestScene::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera&, uint32_t, uint32_t)
{
    dd::xzSquareGrid(-5.0f, 5.0f, 0.0f, 1.0f, dd::colors::LightGray);

    dd::axisTriad(ddConvert(parent->getTransform()->getGlobalMatrix()), 0.3f, 1.0f);
    dd::axisTriad(ddConvert(child->getTransform()->getGlobalMatrix()), 0.2f, 1.0f);

    // RENDER THE SCENE HIERARCHY
    scene->getRoot()->render(cmd);
}

void RenderPipelineTestScene::shutdown()
{
    scene.reset();
}