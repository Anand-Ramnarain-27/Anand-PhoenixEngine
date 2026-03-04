#pragma once
#include "Module.h"
#include "ResourceBase.h"
#include <unordered_map>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>

class ResourceMesh;

class ModuleResources : public Module
{
public:
    ModuleResources();
    ~ModuleResources();

    bool init()    override;
    bool cleanUp() override;

    ResourceBase* RequestResource(UID uid);
    void          ReleaseResource(ResourceBase* resource);

    std::string GetLibraryMeshPath(UID uid) const;

    const std::unordered_map<UID, ResourceBase*>& getLoadedResources() const
    {
        return m_resources;
    }

private:
    ResourceBase* CreateResourceFromUID(UID uid);

    void StartAssetWatcher();
    void StopAssetWatcher();
    void AssetWatcherLoop();

    std::unordered_map<UID, ResourceBase*> m_resources;
    std::mutex                             m_resourceMutex;

    std::filesystem::path  m_assetsPath = "Assets";
    std::filesystem::path  m_libraryPath = "Library";

    std::thread            m_watcherThread;
    std::atomic<bool>      m_watcherRunning = false;
};