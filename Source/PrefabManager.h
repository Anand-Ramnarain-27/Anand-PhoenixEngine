#pragma once

#include <string>
#include <memory>
#include <vector>

class GameObject;
class ModuleScene;

class PrefabManager
{
public:
    static bool createPrefab(const GameObject* go, const std::string& prefabName);
    static GameObject* instantiatePrefab(const std::string& prefabName, ModuleScene* scene);
    static std::vector<std::string> listPrefabs();

    static bool applyToPrefab(const GameObject* go);
    static bool revertToPrefab(GameObject* go);
    static bool isPrefabInstance(const GameObject* go);

    static std::string getPrefabName(const GameObject* go);

private:
    static std::string getPrefabPath(const std::string& prefabName);
};