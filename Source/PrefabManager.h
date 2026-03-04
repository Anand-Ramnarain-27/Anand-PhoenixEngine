#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>

#include "3rdParty/rapidjson/document.h"

class GameObject;
class ModuleScene;

struct PrefabOverrideRecord
{
    std::unordered_map<int, std::unordered_set<std::string>> modifiedProperties;
    std::vector<int> addedComponentTypes;
    std::vector<int> removedComponentTypes;

    bool isEmpty() const
    {
        return modifiedProperties.empty()
            && addedComponentTypes.empty()
            && removedComponentTypes.empty();
    }
    void clear()
    {
        modifiedProperties.clear();
        addedComponentTypes.clear();
        removedComponentTypes.clear();
    }
};

struct PrefabInstanceData
{
    std::string          prefabName;
    uint32_t             prefabUID = 0;
    PrefabOverrideRecord overrides;
};

class PrefabManager
{
public:
    struct PrefabInfo
    {
        std::string name;
        uint32_t    uid = 0;
        int         version = 0;
        int         childCount = 0;
        std::string componentSummary;
        std::string variantOf;
        bool        isVariant = false;
    };

    static bool        createPrefab(const GameObject* go, const std::string& prefabName);
    static GameObject* instantiatePrefab(const std::string& prefabName, ModuleScene* scene);
    static bool        applyToPrefab(const GameObject* go, bool respectOverrides = true);
    static bool        revertToPrefab(GameObject* go, ModuleScene* scene);

    static void markPropertyOverride(GameObject* go, int componentType, const std::string& propertyName);
    static void clearComponentOverrides(GameObject* go, int componentType);
    static void clearAllOverrides(GameObject* go);

    static bool createVariant(const std::string& srcPrefabName, const std::string& dstPrefabName);

    static bool        isPrefabInstance(const GameObject* go);
    static std::string getPrefabName(const GameObject* go);
    static uint32_t    getPrefabUID(const GameObject* go);

    static const PrefabInstanceData* getInstanceData(const GameObject* go);
    static       PrefabInstanceData* getInstanceDataMutable(GameObject* go);

    static std::vector<PrefabInfo>  listPrefabsInfo();
    static std::vector<std::string> listPrefabs();
    static bool                     prefabExists(const std::string& prefabName);

    static void linkInstance(GameObject* go, const PrefabInstanceData& data);
    static void unlinkInstance(GameObject* go);

    static uint32_t makePrefabUID(const std::string& name);

private:
    static std::string getPrefabPath(const std::string& name);

    struct SerialiseCtx;
    static void        serialiseNode(const GameObject* go, SerialiseCtx& ctx);
    static GameObject* deserialiseNode(const rapidjson::Value& node,
        ModuleScene* scene, GameObject* parent);

    static std::unordered_map<const GameObject*, PrefabInstanceData>& registry();
};