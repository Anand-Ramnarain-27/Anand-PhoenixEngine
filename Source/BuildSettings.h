#pragma once
#include <string>
#include <vector>

struct BuildSceneEntry {
    std::string path;
    bool enabled = true;
};

struct BuildSettings {
    std::vector<BuildSceneEntry> scenes;
    std::string outputDir;
    std::string configuration = "Release";
    std::string platform = "x64";

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath) const;

    int getBuildIndex(const std::string& scenePath) const;
    std::string getScenePathAtBuildIndex(int index) const;
    int getEnabledSceneCount() const;
};
