#pragma once
#include "Module.h"
#include "ResourceCommon.h"
#include <string>
#include <vector>
#include <unordered_map>

class ModuleAssets : public Module
{
public:
    struct SceneInfo
    {
        std::string name;
        std::string path;
        uint32_t    meshCount = 0;
        uint32_t    materialCount = 0;
        UID         uid = 0;
    };

    ModuleAssets() = default;
    ~ModuleAssets() override = default;

    bool init()    override;
    bool cleanUp() override;

    UID  importAsset(const char* filePath);
    void refreshAssets();

    void deleteAsset(const std::string& assetPath);

    UID         findUID(const std::string& assetPath) const;
    std::string getPathFromUID(UID uid)               const;
    bool        needsReimport(const std::string& assetPath) const;

    UID findSubUID(const std::string& sceneAssetPath,
        const std::string& type,
        int                index) const;

    std::string getAssetPathForScene(const std::string& sceneName) const;

    bool sceneExists(const std::string& sceneName) const;
    std::vector<SceneInfo> getImportedScenes()     const;

private:
    void ensureLibraryDirectories();

    void registerSceneSubResources(const std::string& filePath,
        const std::string& sceneName,
        int meshCount,
        int materialCount);

    std::unordered_map<std::string, UID> m_pathToUID;
    std::unordered_map<UID, std::string> m_uidToPath;

    std::unordered_map<std::string, UID> m_subUIDs;

    std::unordered_map<std::string, std::string> m_sceneNameToPath;
};