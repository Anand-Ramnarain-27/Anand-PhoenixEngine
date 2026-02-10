#include "Globals.h"
#include "RenderPipelineTestScene.h"
#include "SceneGraph.h"
#include "GameObject.h" 
#include "ComponentTransform.h"
#include "ComponentMeshRenderer.h"

#include "ModuleCamera.h"
#include "DebugDrawPass.h"

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
    scene = std::make_unique<SceneGraph>();

    GameObject* root = scene->getRoot();

    auto cube = std::make_unique<GameObject>("Cube");
    auto* transform = cube->addComponent<ComponentTransform>();
    transform->position = { 0, 0, 0 };

    cube->addComponent<ComponentMeshRenderer>();

    root->addChild(std::move(cube));

    return true;
}

void RenderPipelineTestScene::update(float dt)
{
    scene->update(dt);
}

void RenderPipelineTestScene::render(
    ID3D12GraphicsCommandList* cmd,
    const ModuleCamera& camera,
    uint32_t,
    uint32_t)
{
    scene->render(cmd, camera);
}


void RenderPipelineTestScene::update(float deltaTime)
{
    m_time += deltaTime;
}

void RenderPipelineTestScene::render(ID3D12GraphicsCommandList*, const ModuleCamera& , uint32_t , uint32_t
)
{
    float height = 0.5f + 0.25f * sinf(m_time);

    dd::xzSquareGrid(-5.0f, 5.0f, 0.0f, 1.0f, dd::colors::LightGray);

    dd::axisTriad(ddConvert(Matrix::Identity), 0.2f, 1.0f);

    float start[3] = { 0.0f, 0.0f, 0.0f };
    float end[3] = { 0.0f, height, 0.0f };

    dd::line(dd::colors::Orange, start, end);
}


void RenderPipelineTestScene::shutdown()
{
}
