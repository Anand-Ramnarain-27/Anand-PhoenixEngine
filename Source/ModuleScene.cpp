#include "Globals.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include <algorithm>

ModuleScene::ModuleScene() { root = std::make_unique<GameObject>("Root"); }
ModuleScene::~ModuleScene() = default;

GameObject* ModuleScene::createGameObject(const std::string& name, GameObject* parent){
    auto go = std::make_unique<GameObject>(name);
    auto* ptr = go.get();
    ptr->setParent(parent ? parent : root.get());
    objects.push_back(std::move(go));
    return ptr;
}

void ModuleScene::destroyGameObject(GameObject* go){
    if (!go || go == root.get()) return;
    // Reparent children to go's parent (or root) so the flat objects list stays
    // consistent.  Callers that want recursive deletion (e.g. the editor delete
    // action) must collect and destroy descendants themselves before calling this.
    GameObject* reparentTo = go->getParent() ? go->getParent() : root.get();
    for (auto* child : go->getChildren()) child->setParent(reparentTo);
    go->setParent(nullptr);
    auto it = std::find_if(objects.begin(), objects.end(),
        [go](const std::unique_ptr<GameObject>& p) { return p.get() == go; });
    if (it != objects.end()) objects.erase(it);
}

void ModuleScene::update(float deltaTime) { root->update(deltaTime); }

void ModuleScene::clear() {
    // MUST clear root's raw-pointer children vector BEFORE destroying the
    // unique_ptr<GameObject> objects.  ~GameObject() is = default and does NOT
    // call setParent(nullptr), so after objects.clear() the root would still
    // hold dangling pointers to freed memory.  Any traversal of root->getChildren()
    // on the next frame would then be a use-after-free crash.
    root->clearChildren();
    objects.clear();
}

GameObject* ModuleScene::findGameObjectByName(const std::string& name){
    std::function<GameObject* (GameObject*)> search = [&](GameObject* node) -> GameObject*
        {
            if (node->getName() == name) return node;
            for (auto* child : node->getChildren()) if (auto* found = search(child)) return found;
            return nullptr;
        };
    return search(root.get());
}
