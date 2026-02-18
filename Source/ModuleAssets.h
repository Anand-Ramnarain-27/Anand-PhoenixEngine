#pragma once

#include "Module.h"
#include <string>
#include <vector>

class ModuleAssets : public Module
{
public:
    struct SceneInfo
    {
        std::string name;
        std::string path;
        uint32_t    meshCount = 0;
        uint32_t    materialCount = 0;
    };

    ModuleAssets() = default;
    ~ModuleAssets() override = default;

    bool init()    override;
    bool cleanUp() override;

    void importAsset(const char* filePath);

    std::vector<SceneInfo> getImportedScenes() const;
    bool sceneExists(const std::string& sceneName) const;

private:
    void ensureLibraryDirectories();
};