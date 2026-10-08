#pragma once
// Prefabs: GameObject hierarchies saved as Library/Prefabs/<name>.prefab, their scene instances and overrides.

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "3rdParty/rapidjson/document.h"

class GameObject;
class SceneGraph;

/// What an instance changed relative to its prefab, so apply / revert can skip or keep those edits.
struct PrefabOverrideRecord {
    std::unordered_map<int, std::unordered_set<std::string>> modifiedProperties;
    std::vector<int> addedComponentTypes;
    std::vector<int> removedComponentTypes;

    bool isEmpty() const { return modifiedProperties.empty() && addedComponentTypes.empty() && removedComponentTypes.empty(); }
    void clear(){ modifiedProperties.clear(); addedComponentTypes.clear(); removedComponentTypes.clear(); }
};

/// Link from an instance root in a scene to its prefab file.
struct PrefabInstanceData {
    std::string prefabName;
    uint32_t prefabUID = 0;
    PrefabOverrideRecord overrides;
};

/// Static API over the prefab files and the instance registry (GameObject* -> PrefabInstanceData). Instance links
/// are saved in scenes as "PrefabLink" and restored with linkInstance() on load.
class PrefabManager {
public:
    struct PrefabInfo {
        std::string name;
        uint32_t uid = 0;
        int version = 0;
        int childCount = 0;
        std::string componentSummary;
        std::string variantOf;
        bool isVariant = false;
    };

    static bool createPrefab(const GameObject* go, const std::string& prefabName);
    static GameObject* instantiatePrefab(const std::string& prefabName, SceneGraph* scene);
    /// Writes the instance `go` belongs to back to its prefab file. `respectOverrides` false saves it as-is.
    static bool applyToPrefab(const GameObject* go, bool respectOverrides = true);
    /// Rebuilds the instance `go` belongs to from its prefab file.
    static bool revertToPrefab(GameObject* go, SceneGraph* scene);

    static void markPropertyOverride(GameObject* go, int componentType, const std::string& propertyName);
    static void clearPropertyOverride(GameObject* go, int componentType, const std::string& propertyName);
    // Script fields (ComponentScript): read / write a prefab file by name, and where it lives. Overrides are
    // marked as "Script:<Class>.<field>" under the Script component type.
    static bool readPrefabByName(const std::string& prefabName, rapidjson::Document& doc);
    static bool writePrefabByName(const std::string& prefabName, rapidjson::Document& doc);
    static std::string getPrefabFilePath(const std::string& prefabName);
    static void clearComponentOverrides(GameObject* go, int componentType);
    static void clearAllOverrides(GameObject* go);

    static bool createVariant(const std::string& srcPrefabName, const std::string& dstPrefabName);

    static std::string serializeGameObject(const GameObject* go);
    static GameObject* deserializeGameObject(const std::string& data, SceneGraph* scene);
    // Same, but attached under `parent` instead of the scene root (the plain overload above never restores
    // the original parent, since parentage isn't part of what gets serialized).
    static GameObject* deserializeGameObject(const std::string& data, SceneGraph* scene, GameObject* parent);

    static bool isPrefabInstance(const GameObject* go);
    static std::string getPrefabName(const GameObject* go);
    static uint32_t getPrefabUID(const GameObject* go);

    static const PrefabInstanceData* getInstanceData(const GameObject* go);
    static PrefabInstanceData* getInstanceDataMutable(GameObject* go);

    static std::vector<PrefabInfo> listPrefabsInfo();
    static std::vector<std::string> listPrefabs();
    static bool prefabExists(const std::string& prefabName);

    static void linkInstance(GameObject* go, const PrefabInstanceData& data);
    static void unlinkInstance(GameObject* go);

    /// Stable id from the prefab name (FNV-1a), never 0.
    static uint32_t makePrefabUID(const std::string& name);

    /// Called by GameObject when a component is added / removed; recorded as an override on prefab instances.
    /// Defined in PrefabManagerCore.cpp so they link into GameScript.dll.
    static void markComponentAdded(GameObject* go, int componentType);
    static void markComponentRemoved(GameObject* go, int componentType);

private:
    static std::string getPrefabPath(const std::string& name);
    static bool writePrefabDocument(rapidjson::Document& doc, const std::string& path);
    static bool readPrefabDocument(const std::string& path, rapidjson::Document& doc);

    struct SerialiseCtx;
    static void serialiseNode(const GameObject* go, SerialiseCtx& ctx);
    static GameObject* deserialiseNode(const rapidjson::Value& node, SceneGraph* scene, GameObject* parent);

    static std::unordered_map<const GameObject*, PrefabInstanceData>& registry();
};
