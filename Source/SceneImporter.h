#pragma once

#include <string>
#include <vector>
#include <memory>

class Model;

namespace tinygltf
{
    class Model;
    struct Primitive;
}

class SceneImporter
{
public:
    struct SceneHeader
    {
        uint32_t magic = 0x53434E45;
        uint32_t version = 1;
        uint32_t meshCount = 0;
        uint32_t materialCount = 0;
    };

public:

    static bool ImportFromLoadedGLTF(const tinygltf::Model& gltfModel, const std::string& sceneName);

    static bool LoadScene(const std::string& sceneName, std::unique_ptr<Model>& outModel);

private:
    static std::string GetSceneName(const char* filePath);

    static bool CreateSceneDirectory(const std::string& sceneName);

    static bool SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel);

    static bool LoadSceneMetadata(const std::string& sceneName, SceneHeader& header);
};