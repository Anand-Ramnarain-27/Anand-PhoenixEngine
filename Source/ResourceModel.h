#pragma once
#include "ResourceCommon.h"
#include <vector>
#include <string>

class ModuleScene;
class GameObject;

class ResourceModel : public ResourceBase {
public:
    struct MeshPair {
        UID meshUID     = 0;
        UID materialUID = 0;
    };

    struct Node {
        std::string        name;
        Vector3            translation = {};
        Quaternion         rotation    = Quaternion::Identity;
        Vector3            scale       = { 1.f, 1.f, 1.f };
        int                parentIndex = -1;
        std::vector<MeshPair> meshes;
    };

    explicit ResourceModel(UID uid);

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    const std::vector<Node>& getNodes() const { return m_nodes; }

    // Spawns the model hierarchy into the scene, parented under `parent` (or root if null).
    // Returns the container GameObject created for the model root.
    GameObject* spawnIntoScene(ModuleScene* scene, GameObject* parent = nullptr) const;

private:
    std::vector<Node> m_nodes;
};
