#include "Globals.h"
#include "ModuleScene.h"
#include "GameObject.h"

ModuleScene::ModuleScene()
{
    root = std::make_unique<GameObject>("Root");
    registerGameObject(root.get());
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
    registerGameObject(ptr);
    objects.push_back(std::move(go));
    return ptr;
}

void ModuleScene::update(float deltaTime)
{
    root->update(deltaTime);
}

GameObject* ModuleScene::findGameObjectByUUID(const UUID64& uuid) const
{
    auto it = uuidMap.find(uuid);
    if (it != uuidMap.end())
        return it->second;

    return nullptr;
}

void ModuleScene::clear()
{
    objects.clear();
    uuidMap.clear();

    root = std::make_unique<GameObject>("Root");
    registerGameObject(root.get());
}

void ModuleScene::registerGameObject(GameObject* go)
{
    if (go)
    {
        uuidMap[go->getUUID()] = go;
    }
}