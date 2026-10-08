#pragma once
// The build list and packaging options, stored as Library/BuildSettings.json.

#include <string>
#include <vector>

struct BuildSceneEntry {
    std::string path;
    bool enabled = true;
};

/// The scenes a game ships with (in order; the first enabled one boots) and how File > Build packages them.
/// The Player reads the shipped copy to find its scenes.
struct BuildSettings {
    std::vector<BuildSceneEntry> scenes;
    std::string outputDir;
    std::string configuration = "Release";
    std::string platform = "x64";
    std::string productName = "Player";
    bool stripSourceAssets = false;   // leave out source files already imported into Library/ (models, textures)

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath) const;

    /// Position of `scenePath` among the enabled scenes, or -1.
    int getBuildIndex(const std::string& scenePath) const;
    /// The enabled scene at `index`, or "" when out of range.
    std::string getScenePathAtBuildIndex(int index) const;
    int getEnabledSceneCount() const;
};
