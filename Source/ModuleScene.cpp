#include "Globals.h"
#include "ModuleScene.h"
#include "GameObject.h"

ModuleScene::ModuleScene()
{
    root = std::make_unique<GameObject>("Root");
}

ModuleScene::~ModuleScene() = default;

GameObject* ModuleScene::createGameObject(
    const std::string& name,
    GameObject* parent)
{
    auto go = std::make_unique<GameObject>(name);
    GameObject* ptr = go.get();

    if (!parent)
        parent = root.get();

    ptr->setParent(parent);

    objects.push_back(std::move(go));
    return ptr;
}

void ModuleScene::update(float deltaTime)
{
    root->update(deltaTime);
}
