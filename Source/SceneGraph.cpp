#include "Globals.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include <algorithm>
#include <functional>
#include <cfloat>

SceneGraph::SceneGraph(){ m_root = std::make_unique<GameObject>("Root"); }
SceneGraph::~SceneGraph() = default;

GameObject* SceneGraph::createGameObject(const std::string& name, GameObject* parent){
    auto go = std::make_unique<GameObject>(name);
    auto* ptr = go.get();
    ptr->setParent(parent ? parent : m_root.get());
    m_objects.push_back(std::move(go));
    return ptr;
}

void SceneGraph::destroyGameObject(GameObject* go){
    if (!go || go == m_root.get()) return;
    GameObject* reparentTo = go->getParent() ? go->getParent() : m_root.get();
    for (auto* child : go->getChildren()) child->setParent(reparentTo);
    go->setParent(nullptr);
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
        [go](const std::unique_ptr<GameObject>& p){ return p.get() == go; });
    if (it != m_objects.end()) m_objects.erase(it);
}

void SceneGraph::update(float deltaTime){ m_root->update(deltaTime); }

void SceneGraph::clear(){
    m_root->clearChildren();
    m_objects.clear();
}

GameObject* SceneGraph::findGameObjectByName(const std::string& name){
    std::function<GameObject* (GameObject*)> search = [&](GameObject* node) -> GameObject* {
            if (node->getName() == name) return node;
            for (auto* child : node->getChildren()) if (auto* found = search(child)) return found;
            return nullptr;
        };
    return search(m_root.get());
}

GameObject* SceneGraph::findNearestGameObjectWithTag(const std::string& tag, const Vector3& fromPosition,
                                                     GameObject* exclude){
    GameObject* best = nullptr;
    float bestDistSq = FLT_MAX;

    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (node != m_root.get() && node != exclude && node->isActive() && node->getTag() == tag){
            ComponentTransform* t = node->getTransform();
            if (t){
                float distSq = (t->position - fromPosition).LengthSquared();
                if (distSq < bestDistSq){ bestDistSq = distSq; best = node; }
            }
        }
        for (auto* child : node->getChildren()) visit(child);
    };
    visit(m_root.get());
    return best;
}
