#include "Globals.h"
#include "ModuleScene.h"
#include "GameObject.h"

#include <algorithm>
#include <functional>

ModuleScene::ModuleScene()
{
    root = std::make_unique<GameObject>("Root");
}

ModuleScene::~ModuleScene() = default;

GameObject* ModuleScene::createGameObject(const std::string& name, GameObject* parent)
{
    auto go = std::make_unique<GameObject>(name);
    GameObject* ptr = go.get();

    if (!parent)
        parent = root.get();

    ptr->setParent(parent);
    objects.push_back(std::move(go));
    return ptr;
}

void ModuleScene::destroyGameObject(GameObject* go)
{
    if (!go || go == root.get())
        return;

    GameObject* newParent = go->getParent() ? go->getParent() : root.get();

    std::vector<GameObject*> kids = go->getChildren();
    for (GameObject* child : kids)
        child->setParent(newParent);

    go->setParent(nullptr);

    auto it = std::find_if(objects.begin(), objects.end(),
        [go](const std::unique_ptr<GameObject>& p) { return p.get() == go; });

    if (it != objects.end())
        objects.erase(it);
}

void ModuleScene::update(float deltaTime)
{
    root->update(deltaTime);
}

void ModuleScene::clear()
{
    objects.clear();
    LOG("ModuleScene: Cleared all objects");
}

GameObject* ModuleScene::findGameObjectByName(const std::string& name)
{
    std::function<GameObject* (GameObject*)> search = [&](GameObject* node) -> GameObject*
        {
            if (node->getName() == name)
                return node;
            for (auto* child : node->getChildren())
                if (auto* found = search(child))
                    return found;
            return nullptr;
        };
    return search(root.get());
}