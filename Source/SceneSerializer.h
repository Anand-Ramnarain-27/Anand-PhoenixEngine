#pragma once

#include <string>
#include <memory>

class ModuleScene;
class GameObject;

// Scene Serialization using RapidJSON
// Follows lecture requirements for scene saving/loading
class SceneSerializer
{
public:
    // Save entire scene to file
    static bool SaveScene(const ModuleScene* scene, const std::string& filePath);

    // Load entire scene from file
    static bool LoadScene(const std::string& filePath, ModuleScene* scene);

    // Save scene state for Play/Stop functionality
    static bool SaveTempScene(const ModuleScene* scene);

    // Restore scene from temp save
    static bool LoadTempScene(ModuleScene* scene);

private:
    // Helper methods for serialization
    static std::string SerializeGameObject(const GameObject* go);
    static GameObject* DeserializeGameObject(const std::string& json, ModuleScene* scene);
};