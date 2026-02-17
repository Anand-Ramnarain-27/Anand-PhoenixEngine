#pragma once

#include "ModuleD3D12.h"
#include "MeshPipeline.h"
#include <vector>

class GameObject;
class ComponentDirectionalLight;
class ComponentPointLight;
class ComponentSpotLight;
class EditorSceneSettings;

class LightCollector
{
public:
    static void CollectLights(GameObject* root, const EditorSceneSettings& settings, MeshPipeline::LightCB& outLightData);

private:
    static void CollectLightsRecursive(GameObject* obj, std::vector<ComponentDirectionalLight*>& dirLights, std::vector<ComponentPointLight*>& pointLights, std::vector<ComponentSpotLight*>& spotLights);
};