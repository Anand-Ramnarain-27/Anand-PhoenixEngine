#pragma once
#include "Module.h"
#include "ResourceBase.h"
#include <unordered_map>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

class ModuleResources : public Module
{
public:
    ModuleResources();
    ~ModuleResources();

    bool init()    override;
    bool cleanUp() override;

    ResourceBase* RequestResource(UID uid);

    void ReleaseResource(ResourceBase* resource);

    class ResourceMesh* RequestMesh(UID uid);
    class ResourceMaterial* RequestMaterial(UID uid);
    class ResourceTexture* RequestTexture(UID uid);

    void registerMesh(UID uid, const std::string& libraryPath);
    void registerMaterial(UID uid, const std::string& libraryPath, UID textureUID = 0);
    void registerTexture(UID uid, const std::string& libraryPath);

    const std::unordered_map<UID, ResourceBase*>& getLoadedResources() const { return m_resources; }

    std::string getLibraryPath(UID uid) const;

private:
    ResourceBase* CreateResourceFromUID(UID uid);

    void StartAssetWatcher();
    void StopAssetWatcher();
    void AssetWatcherLoop();

    std::unordered_map<UID, ResourceBase*> m_resources;
    std::mutex                             m_resourceMutex;

    struct ResourceRecord
    {
        std::string libraryPath;
        ResourceBase::Type type = ResourceBase::Type::Unknown;
        UID textureUID = 0;  
    };
    std::unordered_map<UID, ResourceRecord> m_registry;

    std::filesystem::path m_assetsPath = "Assets";
    std::filesystem::path m_libraryPath = "Library";

    std::thread       m_watcherThread;
    std::atomic<bool> m_watcherRunning = false;
};