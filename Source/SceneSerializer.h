#pragma once
#include <string>

class SceneGraph;

class SceneSerializer {
public:
    static bool SaveScene(const SceneGraph* scene, const std::string& filePath);
    static bool LoadScene(const std::string& filePath, SceneGraph* scene);
    static bool SaveTempScene(const SceneGraph* scene);
    static bool LoadTempScene(SceneGraph* scene);
};
