#include "Globals.h"
#include "ResourceModel.h"
#include "SceneImporter.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "ModuleScene.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "GameObject.h"
#include <filesystem>

ResourceModel::ResourceModel(UID uid) : ResourceBase(uid, Type::Model) {}

bool ResourceModel::LoadInMemory() {
    m_nodes.clear();

    std::string sceneName = std::filesystem::path(assetsFile).stem().string();

    std::vector<SceneImporter::NodeInfo> nodeInfos;
    if (!SceneImporter::LoadNodeTree(sceneName, nodeInfos)) {
        LOG("ResourceModel: Failed to load node tree for '%s'", sceneName.c_str());
        return false;
    }

    std::vector<int> matIndices;
    SceneImporter::LoadMaterialIndices(sceneName, matIndices);

    m_nodes.reserve(nodeInfos.size());
    for (const auto& ni : nodeInfos) {
        Node node;
        node.name        = ni.name;
        node.translation = ni.translation;
        node.rotation    = ni.rotation;
        node.scale       = ni.scale;
        node.parentIndex = ni.parentIndex;

        for (int j = ni.meshFileStart; j < ni.meshFileStart + ni.meshFileCount; ++j) {
            MeshPair pair;
            pair.meshUID = app->getAssets()->findSubUID(assetsFile, "mesh", j);

            int matIdx = (j >= 0 && j < (int)matIndices.size()) ? matIndices[j] : -1;
            if (matIdx >= 0)
                pair.materialUID = app->getAssets()->findSubUID(assetsFile, "mat", matIdx);

            node.meshes.push_back(pair);
        }

        m_nodes.push_back(std::move(node));
    }

    return !m_nodes.empty();
}

void ResourceModel::UnloadFromMemory() {
    m_nodes.clear();
}

GameObject* ResourceModel::spawnIntoScene(ModuleScene* scene, GameObject* parent) const {
    if (!scene || m_nodes.empty()) return nullptr;

    std::string modelName = std::filesystem::path(assetsFile).stem().string();
    GameObject* root = scene->createGameObject(modelName, parent);

    std::vector<GameObject*> goList(m_nodes.size(), nullptr);

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        std::string nodeName = n.name.empty() ? ("Node_" + std::to_string(i)) : n.name;
        GameObject* go = scene->createGameObject(nodeName);

        ComponentTransform* t = go->getTransform();
        if (t) {
            t->position = n.translation;
            t->rotation = n.rotation;
            t->scale    = n.scale;
            t->markDirty();
        }

        if (!n.meshes.empty()) {
            auto* meshComp = go->createComponent<ComponentMesh>();
            for (const auto& pair : n.meshes)
                meshComp->addMeshEntry(pair.meshUID, pair.materialUID);
            meshComp->computeLocalAABB();
        }

        goList[i] = go;
    }

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (!goList[i]) continue;
        int p = m_nodes[i].parentIndex;
        if (p >= 0 && p < (int)goList.size() && goList[p])
            goList[i]->setParent(goList[p]);
        else
            goList[i]->setParent(root);
    }

    return root;
}
