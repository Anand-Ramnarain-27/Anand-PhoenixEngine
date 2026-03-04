#pragma once
#include "Module.h"
#include "MetaFileManager.h"  
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

    UID         findUID(const std::string& assetPath) const;
    std::string getPathFromUID(UID uid)               const;
    bool        needsReimport(const std::string& assetPath) const;

    bool sceneExists(const std::string& sceneName) const;
    std::vector<SceneInfo> getImportedScenes()     const;

private:
    void ensureLibraryDirectories();

    std::unordered_map<std::string, UID> m_pathToUID;
    std::unordered_map<UID, std::string> m_uidToPath;
};