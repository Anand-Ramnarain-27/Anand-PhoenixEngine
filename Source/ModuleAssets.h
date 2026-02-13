#pragma once

#include "Module.h"
#include <string>
#include <vector>

class ModuleAssets : public Module
{
public:
    ModuleAssets();
    ~ModuleAssets() override;

    bool init() override;
    bool cleanUp() override;

    void importAsset(const char* filePath);

    struct SceneInfo
    {
        std::string name;
        uint32_t meshCount;
        uint32_t materialCount;
        std::string path;
    };

    std::vector<SceneInfo> getImportedScenes() const;
    bool sceneExists(const std::string& sceneName) const;

private:
    void ensureLibraryDirectories();
};