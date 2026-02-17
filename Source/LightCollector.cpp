#include "Globals.h"
#include "LightCollector.h"
#include "GameObject.h"
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"
#include "ComponentTransform.h"
#include "EditorSceneSettings.h"
#include <algorithm>

void LightCollector::CollectLights(GameObject* root, const EditorSceneSettings& settings, MeshPipeline::LightCB& outLightData)
{
    memset(&outLightData, 0, sizeof(MeshPipeline::LightCB));

    outLightData.ambientColor = settings.ambient.color;
    outLightData.ambientIntensity = settings.ambient.intensity;

    std::vector<ComponentDirectionalLight*> dirLights;
    std::vector<ComponentPointLight*> pointLights;
    std::vector<ComponentSpotLight*> spotLights;

    if (root)
    {
        CollectLightsRecursive(root, dirLights, pointLights, spotLights);
    }

    outLightData.numDirLights = 0;
    for (size_t i = 0; i < dirLights.size() && i < 2; ++i)
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

    outLightData.numPointLights = 0;
    if (!pointLights.empty())
    {
        ComponentPointLight* light = pointLights[0];
        if (light->isEnabled())
        {
            auto* transform = light->getOwner()->getTransform();
            if (transform)
            {
                MeshPipeline::GPUPointLight& gpuLight = outLightData.pointLight;
                gpuLight.position = transform->position;
                gpuLight.color = light->getColor();
                gpuLight.intensity = light->getIntensity();
                gpuLight.sqRadius = light->getRadius() * light->getRadius();

                outLightData.numPointLights = 1;
            }
        }
    }

    outLightData.numSpotLights = 0;
    if (!spotLights.empty())
    {
        ComponentSpotLight* light = spotLights[0];
        if (light->isEnabled())
        {
            auto* transform = light->getOwner()->getTransform();
            if (transform)
            {
                MeshPipeline::GPUSpotLight& gpuLight = outLightData.spotLight;
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

                outLightData.numSpotLights = 1;
            }
        }
    }
}

void LightCollector::CollectLightsRecursive(GameObject* obj, std::vector<ComponentDirectionalLight*>& dirLights, std::vector<ComponentPointLight*>& pointLights, std::vector<ComponentSpotLight*>& spotLights)
{
    if (!obj)
        return;

    const auto& components = obj->getComponents();
    for (const auto& comp : components)
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
    {
        CollectLightsRecursive(child, dirLights, pointLights, spotLights);
    }
}