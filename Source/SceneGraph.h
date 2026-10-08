#pragma once
// Owner of a scene's GameObjects and their hierarchy.

#include "Globals.h"
#include <memory>
#include <vector>
#include <string>

class GameObject;

/// Owns every GameObject of one scene (flat list) under a single invisible root. Parent/child links only order
/// the hierarchy; destroying an object hands its children to its parent.
class SceneGraph {
public:
    SceneGraph();
    ~SceneGraph();

    GameObject* getRoot() const { return m_root.get(); }

    /// `parent` null = under the root.
    GameObject* createGameObject(const std::string& name, GameObject* parent = nullptr);
    /// Immediately; its children move up to its parent. Never call while the scene is updating.
    void destroyGameObject(GameObject* go);
    void update(float deltaTime);
    void clear();
    /// Depth-first, first match.
    GameObject* findGameObjectByName(const std::string& name);
    GameObject* findNearestGameObjectWithTag(const std::string& tag, const Vector3& fromPosition,
                                             GameObject* exclude = nullptr);

private:
    std::unique_ptr<GameObject> m_root;
    std::vector<std::unique_ptr<GameObject>> m_objects;
};
