#pragma once
#include "ResourceCommon.h"
#include <vector>
#include <string>

class ModuleScene;
class GameObject;

class ResourceModel : public ResourceBase {
public:
    struct MeshPair {
        UID meshUID = 0;
        UID materialUID = 0;
    };

    struct Node {
        std::string name;
        Vector3 translation = {};
        Quaternion rotation = Quaternion::Identity;
        Vector3 scale = { 1.f, 1.f, 1.f };
        int parentIndex = -1;
        int skinIndex = -1; // index into m_skins; -1 if not skinned
        std::vector<MeshPair> meshes;
    };

    struct Skin {
        std::string name;
        std::vector<int> jointNodeIndices; // indices into m_nodes
        std::vector<Matrix> inverseBindMatrices; // row-major, one per joint
    };

    explicit ResourceModel(UID uid);

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    const std::vector<Node>& getNodes() const { return m_nodes; }
    const std::vector<Skin>& getSkins() const { return m_skins; }

    // Spawns the model hierarchy into the scene, parented under `parent` (or root if null).
    // Returns the container GameObject created for the model root.
    GameObject* spawnIntoScene(ModuleScene* scene, GameObject* parent = nullptr) const;

private:
    std::vector<Node> m_nodes;
    std::vector<Skin> m_skins;
};
