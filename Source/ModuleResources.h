#pragma once
#include "Module.h"
#include "ResourceCommon.h"
#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

class ResourceBase;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceModel;
class ResourceAnimation;
struct ID3D12GraphicsCommandList;
class ModuleStaticBuffer;

// Constructs a Mesh/Material/Texture/Model resource (never Animation - that
// case is handled inline in ModuleResources::CreateResourceFromUID, which
// doesn't need this). Defined in ModuleResourcesFactories.cpp (the real
// implementation, Engine/Player-only - touches ModuleStaticBuffer/GPU-facing
// code) or ModuleResourcesFactoriesStub.cpp (a nullptr stub, PhoenixCore-only
// - GameScript.dll only ever requests Animation resources today). Exists so
// CreateResourceFromUID's link doesn't need ResourceMesh/Material/Texture/
// Model's constructors just to resolve the Animation case.
ResourceBase* CreateNonAnimationResource(ResourceBase::Type type, UID uid,
    const std::string& libraryPath, UID textureUID, const std::string& assetPath);

class ModuleResources : public Module {
public:
    ModuleResources();
    ~ModuleResources();

    bool init() override;
    bool cleanUp() override;

    ResourceBase* RequestResource(UID uid);
    void ReleaseResource(ResourceBase* resource);

    ResourceMesh* RequestMesh(UID uid);
    ResourceMaterial* RequestMaterial(UID uid);
    ResourceTexture* RequestTexture(UID uid);
    ResourceModel* RequestModel(UID uid);
    ResourceAnimation* RequestAnimation(UID uid);

    void registerMesh(UID uid, const std::string& libraryPath);
    void registerMaterial(UID uid, const std::string& libraryPath, UID textureUID = 0);
    void registerTexture(UID uid, const std::string& libraryPath);
    void registerModel(UID uid, const std::string& libraryPath);
    void registerAnimation(UID uid, const std::string& libraryPath);

    void uploadPendingMeshes(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);

    const std::unordered_map<UID, ResourceBase*>& getLoadedResources() const { return m_resources; }
    std::string getLibraryPath(UID uid) const;

private:
    struct ResourceRecord {
        std::string libraryPath;
        ResourceBase::Type type = ResourceBase::Type::Unknown;
        UID textureUID = 0;
    };

    ResourceBase* CreateResourceFromUID(UID uid);
    void StartAssetWatcher();
    void StopAssetWatcher();
    void AssetWatcherLoop();

    std::unordered_map<UID, ResourceBase*> m_resources;
    std::unordered_map<UID, ResourceRecord> m_registry;
    std::mutex m_resourceMutex;
    std::thread m_watcherThread;
    std::atomic<bool> m_watcherRunning = false;
};
