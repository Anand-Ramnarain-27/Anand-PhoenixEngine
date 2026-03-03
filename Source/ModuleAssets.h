#pragma once
#include "Module.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using UID = uint64_t;

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

    UID importAsset(const char* filePath);

    void refreshAssets();

    std::vector<SceneInfo> getImportedScenes() const;
    bool sceneExists(const std::string& sceneName) const;

    UID findUID(const std::string& assetPath) const;

    bool needsReimport(const std::string& assetPath) const;

    void requestResource(UID uid);
    void releaseResource(UID uid);

private:
    void ensureLibraryDirectories();

    struct MetaData
    {
        UID      uid = 0;
        uint64_t lastModified = 0; 
    };

    bool        saveMeta(const std::string& assetPath, const MetaData& meta) const;
    bool        loadMeta(const std::string& assetPath, MetaData& outMeta)    const;
    UID         getOrCreateUID(const std::string& assetPath);
    static UID  generateUID();
    static uint64_t getLastModified(const std::string& filePath);

    std::unordered_map<std::string, UID> m_pathToUID;

    std::unordered_map<UID, int> m_refCounts;
};