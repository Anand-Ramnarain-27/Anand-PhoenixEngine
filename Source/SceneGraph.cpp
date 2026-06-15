#include "Globals.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include <algorithm>

SceneGraph::SceneGraph(){ root = std::make_unique<GameObject>("Root"); }
SceneGraph::~SceneGraph() = default;

GameObject* SceneGraph::createGameObject(const std::string& name, GameObject* parent){
    auto go = std::make_unique<GameObject>(name);
    auto* ptr = go.get();
    ptr->setParent(parent ? parent : root.get());
    objects.push_back(std::move(go));
    return ptr;
}

void SceneGraph::destroyGameObject(GameObject* go){
    if (!go || go == root.get()) return;
    GameObject* reparentTo = go->getParent() ? go->getParent() : root.get();
    for (auto* child : go->getChildren()) child->setParent(reparentTo);
    go->setParent(nullptr);
    auto it = std::find_if(objects.begin(), objects.end(),
        [go](const std::unique_ptr<GameObject>& p){ return p.get() == go; });
    if (it != objects.end()) objects.erase(it);
}

void SceneGraph::update(float deltaTime){ root->update(deltaTime); }

void SceneGraph::clear(){
    root->clearChildren();
    objects.clear();
}

GameObject* SceneGraph::findGameObjectByName(const std::string& name){
    std::function<GameObject* (GameObject*)> search = [&](GameObject* node) -> GameObject* {
            if (node->getName() == name) return node;
            for (auto* child : node->getChildren()) if (auto* found = search(child)) return found;
            return nullptr;
        };
    return search(root.get());
}
