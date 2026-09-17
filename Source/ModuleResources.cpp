#include "Globals.h"
#include "ModuleResources.h"
#include "MetaFileManager.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ResourceTexture.h"
#include "ResourceModel.h"
#include "ResourceAnimation.h"
#include "ModuleStaticBuffer.h"
#include "ModuleAssets.h"
#include "Application.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

ModuleResources::ModuleResources() = default;
ModuleResources::~ModuleResources() = default;

bool ModuleResources::init(){
    return true;
}

bool ModuleResources::cleanUp(){
    StopAssetWatcher();
    for (auto& [uid, res] : m_resources){ res->UnloadFromMemory(); delete res; }
    m_resources.clear();
    m_registry.clear();
    return true;
}

void ModuleResources::registerMesh(UID uid, const std::string& libraryPath){ m_registry[uid] = { libraryPath, ResourceBase::Type::Mesh, 0 }; }
void ModuleResources::registerMaterial(UID uid, const std::string& libraryPath, UID textureUID){ m_registry[uid] = { libraryPath, ResourceBase::Type::Material, textureUID }; }
void ModuleResources::registerTexture(UID uid, const std::string& libraryPath){ m_registry[uid] = { libraryPath, ResourceBase::Type::Texture, 0 }; }
void ModuleResources::registerModel(UID uid, const std::string& libraryPath){ m_registry[uid] = { libraryPath, ResourceBase::Type::Model, 0 }; }
void ModuleResources::registerAnimation(UID uid, const std::string& libraryPath){ m_registry[uid] = { libraryPath, ResourceBase::Type::Animation, 0 }; }

std::string ModuleResources::getLibraryPath(UID uid) const{
    auto it = m_registry.find(uid);
    return it != m_registry.end() ? it->second.libraryPath : "";
}

// ModuleResources::RequestResource()/ReleaseResource()/RequestAnimation()/
// CreateResourceFromUID() live in ModuleResourcesCore.cpp now - kept separate
// so they can be linked into GameScript.dll (via PhoenixCore) without this
// class's constructor/vtable (init()/cleanUp() are virtual overrides, and
// cleanUp() calls StopAssetWatcher() below - GameScript.dll never
// constructs/destroys a ModuleResources itself, only calls RequestAnimation
// on an already-running instance via app->getResources()).

ResourceMesh* ModuleResources::RequestMesh(UID uid){ return static_cast<ResourceMesh*>(RequestResource(uid)); }
ResourceMaterial* ModuleResources::RequestMaterial(UID uid){ return static_cast<ResourceMaterial*>(RequestResource(uid)); }
ResourceTexture* ModuleResources::RequestTexture(UID uid){ return static_cast<ResourceTexture*>(RequestResource(uid)); }
ResourceModel* ModuleResources::RequestModel(UID uid){ return static_cast<ResourceModel*>(RequestResource(uid)); }

void ModuleResources::uploadPendingMeshes(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer){
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    for (auto& [uid, res] : m_resources){
        if (res->type != ResourceBase::Type::Mesh) continue;
        auto* rm = static_cast<ResourceMesh*>(res);
        if (rm->getMesh() && !rm->isOnGPU()) rm->LoadInMemory(cmd, staticBuffer);
    }
}

// ModuleResources::CreateResourceFromUID() lives in ModuleResourcesCore.cpp now.

void ModuleResources::StartAssetWatcher(){
    m_watcherRunning = true;
    m_watcherThread = std::thread(&ModuleResources::AssetWatcherLoop, this);
}

void ModuleResources::StopAssetWatcher(){
    m_watcherRunning = false;
    if (m_watcherThread.joinable()) m_watcherThread.join();
}

void ModuleResources::AssetWatcherLoop(){
    static constexpr std::string_view kMetaExt = ".meta";
    while (m_watcherRunning){
        try {
            std::string assetsPath = app->getFileSystem()->GetAssetsPath();
            for (auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)){
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == kMetaExt) continue;
                std::string path = entry.path().string();
                if (app->getAssets()->needsReimport(path)){ LOG("ModuleResources: Watcher: reimporting %s", path.c_str()); app->getAssets()->importAsset(path.c_str()); }
            }
        }
        catch (const std::exception& e){ LOG("ModuleResources: Watcher error: %s", e.what()); }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
