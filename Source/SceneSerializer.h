#pragma once
#include <string>
#include "EditorSceneSettings.h"

class SceneGraph;

class SceneSerializer {
public:
    static bool SaveScene(const SceneGraph* scene, const std::string& filePath, const EditorSceneSettings* settings = nullptr);
    static bool LoadScene(const std::string& filePath, SceneGraph* scene, EditorSceneSettings* settings = nullptr);
    static bool SaveTempScene(const SceneGraph* scene);
    static bool LoadTempScene(SceneGraph* scene);
};
