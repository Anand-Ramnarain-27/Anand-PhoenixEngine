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

void ModuleScene::clear()
{
    // Clear all objects but keep root
    objects.clear();

    // Note: This will leave dangling pointers in root's children vector
    // You may need to add a clearChildren() method to GameObject
    LOG("ModuleScene: Cleared all objects");
}

GameObject* ModuleScene::findGameObjectByName(const std::string& name)
{
    std::function<GameObject* (GameObject*)> search = [&](GameObject* go) -> GameObject*
        {
            if (go->getName() == name)
                return go;

            for (auto* child : go->getChildren())
            {
                GameObject* found = search(child);
                if (found)
                    return found;
            }

            return nullptr;
        };

    return search(root.get());
}