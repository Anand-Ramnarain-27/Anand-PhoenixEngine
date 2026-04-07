#include "Globals.h"
#include "ModuleResources.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ResourceTexture.h"
#include "ResourceAnimation.h"
#include "ModuleStaticBuffer.h"
#include "ModuleAssets.h"
#include "Application.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

ModuleResources::ModuleResources() = default;
ModuleResources::~ModuleResources() = default;

bool ModuleResources::init() {
    return true;
}

bool ModuleResources::cleanUp() {
    StopAssetWatcher();
    for (auto& [uid, res] : m_resources) { res->UnloadFromMemory(); delete res; }
    m_resources.clear();
    m_registry.clear();
    return true;
}

void ModuleResources::registerMesh(UID uid, const std::string& libraryPath) { m_registry[uid] = { libraryPath, ResourceBase::Type::Mesh, 0 }; }
void ModuleResources::registerMaterial(UID uid, const std::string& libraryPath, UID textureUID) { m_registry[uid] = { libraryPath, ResourceBase::Type::Material, textureUID }; }
void ModuleResources::registerTexture(UID uid, const std::string& libraryPath) { m_registry[uid] = { libraryPath, ResourceBase::Type::Texture, 0 }; }
void ModuleResources::registerAnimation(UID uid, const std::string& libraryPath) { m_registry[uid] = { libraryPath, ResourceBase::Type::Animation, 0 }; }

std::string ModuleResources::getLibraryPath(UID uid) const {
    auto it = m_registry.find(uid);
    return it != m_registry.end() ? it->second.libraryPath : "";
}

ResourceBase* ModuleResources::RequestResource(UID uid) {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    auto it = m_resources.find(uid);
    if (it != m_resources.end()) { it->second->addRef(); return it->second; }
    ResourceBase* resource = CreateResourceFromUID(uid);
    if (!resource) return nullptr;
    if (!resource->LoadInMemory()) { delete resource; return nullptr; }
    resource->addRef();
    m_resources[uid] = resource;
    return resource;
}

void ModuleResources::ReleaseResource(ResourceBase* resource) {
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    resource->releaseRef();
    if (resource->referenceCount == 0) { resource->UnloadFromMemory(); m_resources.erase(resource->uid); delete resource; }
}

ResourceMesh* ModuleResources::RequestMesh(UID uid) { return static_cast<ResourceMesh*>(RequestResource(uid)); }
ResourceMaterial* ModuleResources::RequestMaterial(UID uid) { return static_cast<ResourceMaterial*>(RequestResource(uid)); }
ResourceTexture* ModuleResources::RequestTexture(UID uid) { return static_cast<ResourceTexture*>(RequestResource(uid)); }
ResourceAnimation* ModuleResources::RequestAnimation(UID uid) { return static_cast<ResourceAnimation*>(RequestResource(uid)); }

void ModuleResources::uploadPendingMeshes(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer) {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    for (auto& [uid, res] : m_resources) {
        if (res->type != ResourceBase::Type::Mesh) continue;
        auto* rm = static_cast<ResourceMesh*>(res);
        if (rm->getMesh() && !rm->isOnGPU()) rm->LoadInMemory(cmd, staticBuffer);
    }
}

ResourceBase* ModuleResources::CreateResourceFromUID(UID uid) {
    auto regIt = m_registry.find(uid);
    if (regIt != m_registry.end()) {
        const ResourceRecord& rec = regIt->second;
        std::string assetPath = app->getAssets()->getPathFromUID(uid);
        switch (rec.type) {
        case ResourceBase::Type::Mesh: { auto* r = new ResourceMesh(uid); r->libraryFile = rec.libraryPath; r->assetsFile = assetPath; return r; }
        case ResourceBase::Type::Material: { auto* r = new ResourceMaterial(uid); r->libraryFile = rec.libraryPath; r->assetsFile = assetPath; r->textureUID = rec.textureUID; return r; }
        case ResourceBase::Type::Texture: { auto* r = new ResourceTexture(uid); r->libraryFile = rec.libraryPath; r->assetsFile = assetPath; return r; }
        case ResourceBase::Type::Animation: { auto* r = new ResourceAnimation(uid); r->libraryFile = rec.libraryPath; r->assetsFile = assetPath; return r; }
        default: LOG("ModuleResources: Unknown type for uid=%llu", uid); return nullptr;
        }
    }
    std::string assetPath = app->getAssets()->getPathFromUID(uid);
    if (assetPath.empty()) { LOG("ModuleResources: No registry entry and no asset path for uid=%llu", uid); return nullptr; }
    MetaData meta;
    if (!MetaFileManager::load(assetPath, meta)) { LOG("ModuleResources: No meta file for %s", assetPath.c_str()); return nullptr; }
    LOG("ModuleResources: uid=%llu not in registry, cannot create without library path", uid);
    return nullptr;
}

void ModuleResources::StartAssetWatcher() {
    m_watcherRunning = true;
    m_watcherThread = std::thread(&ModuleResources::AssetWatcherLoop, this);
}

void ModuleResources::StopAssetWatcher() {
    m_watcherRunning = false;
    if (m_watcherThread.joinable()) m_watcherThread.join();
}

void ModuleResources::AssetWatcherLoop() {
    static constexpr std::string_view kMetaExt = ".meta";
    while (m_watcherRunning) {
        try {
            std::string assetsPath = app->getFileSystem()->GetAssetsPath();
            for (auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == kMetaExt) continue;
                std::string path = entry.path().string();
                if (app->getAssets()->needsReimport(path)) { LOG("ModuleResources: Watcher: reimporting %s", path.c_str()); app->getAssets()->importAsset(path.c_str()); }
            }
        }
        catch (const std::exception& e) { LOG("ModuleResources: Watcher error: %s", e.what()); }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}