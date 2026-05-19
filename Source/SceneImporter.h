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
        uint32_t version = 2;
        uint32_t meshCount = 0;
        uint32_t materialCount = 0;
    };

    struct NodeInfo {
        std::string name;
        int         parentIndex;   // -1 = root
        int         meshFileStart; // -1 = no mesh
        int         meshFileCount;
        Vector3     translation;
        Quaternion  rotation;
        Vector3     scale;
    };

    static bool ImportFromLoadedGLTF(const tinygltf::Model& gltfModel, const std::string& sceneName, const std::string& basePath);
    static bool LoadScene(const std::string& sceneName, std::unique_ptr<Model>& outModel);
    static bool LoadSceneMetadata(const std::string& sceneName, SceneHeader& header);
    static bool LoadNodeTree(const std::string& sceneName, std::vector<NodeInfo>& outNodes);

private:
    static bool CreateSceneDirectory(const std::string& sceneName);
    static bool SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel);
    static bool SaveNodeMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel);
};