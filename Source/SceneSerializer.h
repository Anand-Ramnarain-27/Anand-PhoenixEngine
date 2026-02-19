#pragma once

#include <string>

class ModuleScene;
class GameObject;

class SceneSerializer
{
public:
    static bool SaveScene(const ModuleScene* scene, const std::string& filePath);
    static bool LoadScene(const std::string& filePath, ModuleScene* scene);

    static bool SaveTempScene(const ModuleScene* scene);
    static bool LoadTempScene(ModuleScene* scene);
};