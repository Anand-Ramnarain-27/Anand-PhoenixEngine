#include "Globals.h"
#include "ModuleResources.h"
#include "ResourceMesh.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "MetaFileManager.h"
#include "ModuleAssets.h"
#include <algorithm>
#include <chrono>

ModuleResources::ModuleResources() {}
ModuleResources::~ModuleResources() {}

bool ModuleResources::init()
{
    StartAssetWatcher();
    return true;
}

bool ModuleResources::cleanUp()
{
    StopAssetWatcher();

    for (auto& pair : m_resources)
    {
        pair.second->UnloadFromMemory();
        delete pair.second;
    }
    m_resources.clear();
    return true;
}

ResourceBase* ModuleResources::RequestResource(UID uid)
{
    std::lock_guard<std::mutex> lock(m_resourceMutex);

    auto it = m_resources.find(uid);
    if (it != m_resources.end())
    {
        it->second->addRef();
        return it->second;
    }

    ResourceBase* resource = CreateResourceFromUID(uid);
    if (!resource) return nullptr;

    if (!resource->LoadInMemory())
    {
        delete resource;
        return nullptr;
    }

    resource->addRef();
    m_resources[uid] = resource;
    return resource;
}

void ModuleResources::ReleaseResource(ResourceBase* resource)
{
    if (!resource) return;

    std::lock_guard<std::mutex> lock(m_resourceMutex);

    resource->releaseRef();

    if (resource->referenceCount == 0)
    {
        resource->UnloadFromMemory();
        m_resources.erase(resource->uid);
        delete resource;
    }
}

ResourceBase* ModuleResources::CreateResourceFromUID(UID uid)
{
    std::string assetPath = app->getAssets()->getPathFromUID(uid);
    if (assetPath.empty())
    {
        LOG("ModuleResources: No asset path for uid=%llu", uid);
        return nullptr;
    }

    MetaData meta;
    if (!MetaFileManager::load(assetPath, meta))
    {
        LOG("ModuleResources: No meta for %s", assetPath.c_str());
        return nullptr;
    }

    switch (meta.type)
    {
    case ResourceBase::Type::Model:
    case ResourceBase::Type::Mesh:
    {
        ResourceMesh* r = new ResourceMesh(uid);
        r->assetsFile = assetPath;
        r->libraryFile = assetPath; 
        return r;
    }
    default:
        LOG("ModuleResources: Unhandled type %d for uid=%llu", (int)meta.type, uid);
        return nullptr;
    }
}

std::string ModuleResources::GetLibraryMeshPath(UID uid) const
{
    return (m_libraryPath / "Meshes" / (std::to_string(uid) + ".mesh")).string();
}

void ModuleResources::StartAssetWatcher()
{
    m_watcherRunning = true;
    m_watcherThread = std::thread(&ModuleResources::AssetWatcherLoop, this);
}

void ModuleResources::StopAssetWatcher()
{
    m_watcherRunning = false;
    if (m_watcherThread.joinable())
        m_watcherThread.join();
}

void ModuleResources::AssetWatcherLoop()
{
    while (m_watcherRunning)
    {
        try
        {
            for (auto& entry : std::filesystem::recursive_directory_iterator(m_assetsPath))
            {
                if (!entry.is_regular_file()) continue;

                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".meta") continue;

                if (app->getAssets()->needsReimport(path))
                {
                    LOG("ModuleResources: Watcher: reimporting %s", path.c_str());
                    app->getAssets()->importAsset(path.c_str());
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG("ModuleResources: Watcher error: %s", e.what());
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}