// ModuleResources::RequestResource()/ReleaseResource()/RequestAnimation()/
// CreateResourceFromUID() - split out of ModuleResources.cpp so they can be
// linked into GameScript.dll (via PhoenixCore) without that class's
// constructor/destructor/vtable (init()/cleanUp() are virtual, and
// cleanUp() calls the asset-watcher thread machinery) or the AssetWatcher/
// uploadPendingMeshes machinery, none of which GameScript.dll needs -
// it only ever calls a method on an already-running ModuleResources via
// app->getResources(), never constructs/destroys one itself.
//
// CreateResourceFromUID's Animation case is handled inline (ResourceAnimation
// is confirmed pure CPU-side, safe in PhoenixCore); the Mesh/Material/
// Texture/Model cases go through CreateNonAnimationResource(), an indirection
// point so this file's link doesn't need those classes' constructors (which
// touch GPU-facing code) just to resolve the Animation case - see
// ModuleResources.h for the declaration and ModuleResourcesFactories.cpp /
// ModuleResourcesFactoriesStub.cpp for the two possible definitions.
#include "Globals.h"
#include "ModuleResources.h"
#include "MetaFileManager.h"
#include "ResourceAnimation.h"
#include "ModuleAssets.h"
#include "Application.h"

ResourceBase* ModuleResources::RequestResource(UID uid){
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    auto it = m_resources.find(uid);
    if (it != m_resources.end()){ it->second->addRef(); return it->second; }
    ResourceBase* resource = CreateResourceFromUID(uid);
    if (!resource) return nullptr;
    if (!resource->LoadInMemory()){ delete resource; return nullptr; }
    resource->addRef();
    m_resources[uid] = resource;
    return resource;
}

void ModuleResources::ReleaseResource(ResourceBase* resource){
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    resource->releaseRef();
    if (resource->referenceCount == 0){ resource->UnloadFromMemory(); m_resources.erase(resource->uid); delete resource; }
}

ResourceAnimation* ModuleResources::RequestAnimation(UID uid){ return static_cast<ResourceAnimation*>(RequestResource(uid)); }

ResourceBase* ModuleResources::CreateResourceFromUID(UID uid){
    auto regIt = m_registry.find(uid);
    if (regIt != m_registry.end()){
        const ResourceRecord& rec = regIt->second;
        std::string assetPath = app->getAssets()->getPathFromUID(uid);
        if (rec.type == ResourceBase::Type::Animation){
            auto* r = new ResourceAnimation(uid);
            r->libraryFile = rec.libraryPath;
            r->assetsFile = assetPath;
            return r;
        }
        ResourceBase* r = CreateNonAnimationResource(rec.type, uid, rec.libraryPath, rec.textureUID, assetPath);
        if (!r) LOG("ModuleResources: Unknown type for uid=%llu", uid);
        return r;
    }
    std::string assetPath = app->getAssets()->getPathFromUID(uid);
    if (assetPath.empty()){ LOG("ModuleResources: No registry entry and no asset path for uid=%llu", uid); return nullptr; }
    MetaData meta;
    if (!MetaFileManager::load(assetPath, meta)){ LOG("ModuleResources: No meta file for %s", assetPath.c_str()); return nullptr; }
    LOG("ModuleResources: uid=%llu not in registry, cannot create without library path", uid);
    return nullptr;
}
