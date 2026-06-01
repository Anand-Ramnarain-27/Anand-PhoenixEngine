#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

class Model;
namespace tinygltf { class Model; struct Primitive; }

class SceneImporter {
public:
    struct SceneHeader {
        uint32_t magic = 0x53434E45;
        // v3: animation library added; any cache with version < 3 triggers a full reimport
        //     so that morph-only animations that were silently skipped get re-exported.
        uint32_t version = 3;
        uint32_t meshCount = 0;
        uint32_t materialCount = 0;
    };

    struct NodeInfo {
        std::string name;
        int parentIndex = -1; // -1 = root
        int meshFileStart = -1; // -1 = no mesh
        int meshFileCount = 0;
        int skinIndex = -1; // -1 = not skinned
        Vector3 translation;
        Quaternion rotation;
        Vector3 scale;
    };

    struct SkinInfo {
        std::string name;
        std::vector<int> jointNodeIndices; // node indices of each joint
        std::vector<Matrix> inverseBindMatrices; // row-major, one per joint
    };

    static bool ImportFromLoadedGLTF(const tinygltf::Model& gltfModel, const std::string& sceneName, const std::string& basePath);
    static bool LoadScene(const std::string& sceneName, std::unique_ptr<Model>& outModel);
    static bool LoadSceneMetadata(const std::string& sceneName, SceneHeader& header);
    static bool LoadNodeTree(const std::string& sceneName, std::vector<NodeInfo>& outNodes);
    static bool LoadMaterialIndices(const std::string& sceneName, std::vector<int>& outMatIndices);
    static bool LoadSkins(const std::string& sceneName, std::vector<SkinInfo>& outSkins);

private:
    static bool CreateSceneDirectory(const std::string& sceneName);
    static bool SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel);
    static bool SaveNodeMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel);
    static bool SaveSkinMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel);
};
