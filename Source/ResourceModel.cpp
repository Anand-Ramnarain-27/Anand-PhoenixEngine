#include "Globals.h"
#include "ResourceModel.h"
#include "SceneImporter.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "ComponentAnimation.h"
#include "ModuleScene.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "GameObject.h"
#include <filesystem>

ResourceModel::ResourceModel(UID uid) : ResourceBase(uid, Type::Model) {}

bool ResourceModel::LoadInMemory(){
    m_nodes.clear();
    m_skins.clear();

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
        node.name = ni.name;
        node.translation = ni.translation;
        node.rotation = ni.rotation;
        node.scale = ni.scale;
        node.parentIndex = ni.parentIndex;
        node.skinIndex = ni.skinIndex;

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

    // Load skin definitions (inverse bind matrices + joint node index arrays).
    std::vector<SceneImporter::SkinInfo> skinInfos;
    if (SceneImporter::LoadSkins(sceneName, skinInfos)) {
        m_skins.reserve(skinInfos.size());
        for (auto& si : skinInfos) {
            Skin skin;
            skin.name = std::move(si.name);
            skin.jointNodeIndices = std::move(si.jointNodeIndices);
            skin.inverseBindMatrices = std::move(si.inverseBindMatrices);
            m_skins.push_back(std::move(skin));
        }
    }

    return !m_nodes.empty();
}

void ResourceModel::UnloadFromMemory(){
    m_nodes.clear();
    m_skins.clear();
}

GameObject* ResourceModel::spawnIntoScene(ModuleScene* scene, GameObject* parent) const{
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
            t->scale = n.scale;
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

    // Wire up skin data for each mesh node that references a skin.
    // The Skin is copied into ComponentMesh so that ResourceModel can be released.
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        if (n.skinIndex < 0 || n.skinIndex >= (int)m_skins.size()) continue;
        if (!goList[i]) continue;
        ComponentMesh* cm = goList[i]->getComponent<ComponentMesh>();
        if (!cm) continue;

        const Skin& skin = m_skins[n.skinIndex];
        std::vector<GameObject*> joints;
        joints.reserve(skin.jointNodeIndices.size());
        for (int ji : skin.jointNodeIndices)
            joints.push_back((ji >= 0 && ji < (int)goList.size()) ? goList[ji] : nullptr);
        cm->setSkinData(skin, std::move(joints));
    }

    // Log node summary so we can see which nodes have skins and which have meshes.
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        LOG("ResourceModel: node[%zu] name='%s' skin=%d meshes=%zu",
            i, n.name.c_str(), n.skinIndex, n.meshes.size());
    }

    // Attach ComponentAnimation to the root if this model has any animations.
    std::vector<UID> animUIDs;
    for (int i = 0; ; ++i) {
        UID uid = app->getAssets()->findSubUID(assetsFile, "anim", i);
        if (uid == 0) break;
        animUIDs.push_back(uid);
    }
    LOG("ResourceModel: spawning '%s' — found %d anim UID(s) via findSubUID",
        modelName.c_str(), (int)animUIDs.size());
    if (!animUIDs.empty()) {
        auto* animComp = root->createComponent<ComponentAnimation>();
        animComp->setAnimationList(animUIDs);
        animComp->OnPlay(animUIDs[0], /*loop=*/true);
        LOG("ResourceModel: ComponentAnimation added to root '%s' — playing anim[0]", modelName.c_str());
    } else {
        LOG("ResourceModel: no anim UIDs — ComponentAnimation NOT created. "
            "Delete Library/Animations/%s/ and re-import to regenerate.",
            modelName.c_str());
    }

    return root;
}
