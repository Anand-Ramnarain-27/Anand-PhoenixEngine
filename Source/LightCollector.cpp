#include "Globals.h"
#include "LightCollector.h"
#include "GameObject.h"
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"
#include "ComponentTransform.h"
#include "EditorSceneSettings.h"
#include <algorithm>

void LightCollector::CollectLights(
    GameObject* root,
    const EditorSceneSettings& settings,
    MeshPipeline::LightCB& outLightData)
{
    memset(&outLightData, 0, sizeof(MeshPipeline::LightCB));

    outLightData.ambientColor = settings.ambient.color;
    outLightData.ambientIntensity = settings.ambient.intensity;

    std::vector<ComponentDirectionalLight*> dirLights;
    std::vector<ComponentPointLight*>       pointLights;
    std::vector<ComponentSpotLight*>        spotLights;

    if (root)
        CollectLightsRecursive(root, dirLights, pointLights, spotLights);

    // --- Directional lights ---
    outLightData.numDirLights = 0;
    for (size_t i = 0; i < dirLights.size() && i < MeshPipeline::MAX_DIR_LIGHTS; ++i)
    {
        ComponentDirectionalLight* light = dirLights[i];
        if (!light->isEnabled())
            continue;

        MeshPipeline::GPUDirectionalLight& gpuLight = outLightData.dirLights[outLightData.numDirLights];
        gpuLight.direction = light->getDirection();
        gpuLight.direction.Normalize();
        gpuLight.color = light->getColor();
        gpuLight.intensity = light->getIntensity();

        outLightData.numDirLights++;
    }

    // --- Point lights ---
    outLightData.numPointLights = 0;
    for (size_t i = 0; i < pointLights.size() && i < MeshPipeline::MAX_POINT_LIGHTS; ++i)
    {
        ComponentPointLight* light = pointLights[i];
        if (!light->isEnabled())
            continue;

        auto* transform = light->getOwner()->getTransform();
        if (!transform)
            continue;

        MeshPipeline::GPUPointLight& gpuLight = outLightData.pointLights[outLightData.numPointLights];
        gpuLight.position = transform->position;
        gpuLight.color = light->getColor();
        gpuLight.intensity = light->getIntensity();
        gpuLight.sqRadius = light->getRadius() * light->getRadius();

        outLightData.numPointLights++;
    }

    // --- Spot lights ---
    outLightData.numSpotLights = 0;
    for (size_t i = 0; i < spotLights.size() && i < MeshPipeline::MAX_SPOT_LIGHTS; ++i)
    {
        ComponentSpotLight* light = spotLights[i];
        if (!light->isEnabled())
            continue;

        auto* transform = light->getOwner()->getTransform();
        if (!transform)
            continue;

        MeshPipeline::GPUSpotLight& gpuLight = outLightData.spotLights[outLightData.numSpotLights];
        gpuLight.position = transform->position;
        gpuLight.direction = light->getDirection();
        gpuLight.direction.Normalize();
        gpuLight.color = light->getColor();
        gpuLight.intensity = light->getIntensity();
        gpuLight.sqRadius = light->getRadius() * light->getRadius();

        float innerRad = light->getInnerAngle() * 0.0174532925f;
        float outerRad = light->getOuterAngle() * 0.0174532925f;
        gpuLight.innerCos = cosf(innerRad);
        gpuLight.outerCos = cosf(outerRad);

        outLightData.numSpotLights++;
    }
}

void LightCollector::CollectLightsRecursive(
    GameObject* obj,
    std::vector<ComponentDirectionalLight*>& dirLights,
    std::vector<ComponentPointLight*>& pointLights,
    std::vector<ComponentSpotLight*>& spotLights)
{
    if (!obj)
        return;

    for (const auto& comp : obj->getComponents())
    {
        switch (comp->getType())
        {
        case Component::Type::DirectionalLight:
            dirLights.push_back(static_cast<ComponentDirectionalLight*>(comp.get()));
            break;
        case Component::Type::PointLight:
            pointLights.push_back(static_cast<ComponentPointLight*>(comp.get()));
            break;
        case Component::Type::SpotLight:
            spotLights.push_back(static_cast<ComponentSpotLight*>(comp.get()));
            break;
        default:
            break;
        }
    }

    for (GameObject* child : obj->getChildren())
        CollectLightsRecursive(child, dirLights, pointLights, spotLights);
}