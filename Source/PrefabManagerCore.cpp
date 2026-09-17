// The subset of PrefabManager that GameObject::createComponent<T>() actually
// calls (registry() + markComponentAdded/Removed, which createComponent<T>()
// calls unconditionally for every component type it instantiates - see
// GameObject.cpp). Split out of PrefabManager.cpp so that a thin consumer of
// GameObject.cpp (PhoenixCore / GameScript.dll) doesn't have to link the rest
// of PrefabManager.cpp's dependencies (Application/app, ModuleFileSystem,
// SceneGraph, ComponentFactory) just to satisfy this one whole-object-file
// pull. Compiled into PhoenixCore, Engine.vcxproj, and Player.vcxproj; the
// remaining PrefabManager.cpp functions stay in Engine/Player only.
#include "Globals.h"
#include "PrefabManager.h"
#include "GameObject.h"
#include <algorithm>

std::unordered_map<const GameObject*, PrefabInstanceData>& PrefabManager::registry(){
    static std::unordered_map<const GameObject*, PrefabInstanceData> s_registry;
    return s_registry;
}

PrefabInstanceData* PrefabManager::getInstanceDataMutable(GameObject* go){
    auto it = registry().find(go);
    return (it != registry().end()) ? &it->second : nullptr;
}

void PrefabManager::markComponentAdded(GameObject* go, int componentType){
    PrefabInstanceData* inst = getInstanceDataMutable(go);
    if (!inst) return;

    auto& v = inst->overrides.addedComponentTypes;
    if (std::find(v.begin(), v.end(), componentType) == v.end())
        v.push_back(componentType);

    auto& r = inst->overrides.removedComponentTypes;
    r.erase(std::remove(r.begin(), r.end(), componentType), r.end());
}

void PrefabManager::markComponentRemoved(GameObject* go, int componentType){
    PrefabInstanceData* inst = getInstanceDataMutable(go);
    if (!inst) return;

    auto& v = inst->overrides.removedComponentTypes;
    if (std::find(v.begin(), v.end(), componentType) == v.end())
        v.push_back(componentType);

    auto& a = inst->overrides.addedComponentTypes;
    a.erase(std::remove(a.begin(), a.end(), componentType), a.end());

    inst->overrides.modifiedProperties.erase(componentType);
}
