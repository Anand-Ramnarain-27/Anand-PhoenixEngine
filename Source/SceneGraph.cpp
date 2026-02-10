#include "Globals.h"
#include "SceneGraph.h"
#include "GameObject.h"

SceneGraph::SceneGraph()
{
    root = std::make_unique<GameObject>("Root");
}

SceneGraph::~SceneGraph() = default;

void SceneGraph::update(float dt)
{
    root->update(dt);
}

void SceneGraph::render(
    ID3D12GraphicsCommandList* cmd,
    const ModuleCamera& camera)
{
    root->render(cmd, camera, Matrix::Identity);
}
