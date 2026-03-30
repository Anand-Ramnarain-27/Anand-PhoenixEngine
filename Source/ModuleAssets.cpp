#include "Globals.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"
#include "SceneImporter.h"
#include "TextureImporter.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static bool isModelExtension(const std::string& ext) {
    return ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".stl" || ext == ".blend";
}

static bool isTextureExtension(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".bmp" || ext == ".hdr";
}

static bool isSupportedExtension(const std::string& ext) {
    return isModelExtension(ext) || isTextureExtension(ext);
}

static UID makeSubUID(UID parentUID, const std::string& type, int index) {
    std::string key = std::to_string(parentUID) + "|" + type + "|" + std::to_string(index);
    UID hash = 14695981039346656037ULL;
    for (char c : key) {
        hash ^= (uint8_t)c;
        hash *= 1099511628211ULL;
    }
    return hash ? hash : 1;
}

static std::string normalisePath(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}

bool ModuleAssets::init() {
    ensureLibraryDirectories();
    refreshAssets();

    std::string assetsRoot = app->getFileSystem()->GetAssetsPath();
    m_watcher.start(assetsRoot, [this](const std::string& path, FileWatcher::Event ev) {
        onAssetFileEvent(path, ev);
        });
    return true;
}

void ModuleAssets::update() {
    m_watcher.poll();
}

bool ModuleAssets::cleanUp() {
    m_watcher.stop();
    m_pathToUID.clear();
    m_uidToPath.clear();
    m_subUIDs.clear();
    m_sceneNameToPath.clear();
    return true;
}

void ModuleAssets::onAssetFileEvent(const std::string& absPath, FileWatcher::Event ev) {
    std::string path = normalisePath(absPath);

    fs::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".meta" || !isSupportedExtension(ext)) return;

    switch (ev) {
    case FileWatcher::Event::Added:
    case FileWatcher::Event::Modified:
        if (needsReimport(path)) {
            LOG("ModuleAssets: Detected change, re-importing %s", path.c_str());
            importAsset(path.c_str());
        }
        break;
    case FileWatcher::Event::Deleted:
        LOG("ModuleAssets: Detected deletion: %s", path.c_str());
        deleteAsset(path);
        break;
    }
}

void ModuleAssets::ensureLibraryDirectories() {
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string lib = fsys->GetLibraryPath();
    fsys->CreateDir(lib.c_str());
    fsys->CreateDir((lib + "Meshes").c_str());
    fsys->CreateDir((lib + "Materials").c_str());
    fsys->CreateDir((lib + "Textures").c_str());
    fsys->CreateDir((lib + "Scenes").c_str());
}

void ModuleAssets::countLibraryFiles(const std::string& folder, const std::string& ext, int& count) const {
    count = 0;
    while (app->getFileSystem()->Exists((folder + std::to_string(count) + ext).c_str())) ++count;
}

void ModuleAssets::refreshAssets() {
    std::string assetsRoot = app->getFileSystem()->GetAssetsPath();
    if (!fs::exists(assetsRoot)) return;

    for (const auto& entry : fs::recursive_directory_iterator(assetsRoot)) {
        if (!entry.is_regular_file()) continue;

        std::string path = normalisePath(entry.path().string());

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".meta" || !isSupportedExtension(ext)) continue;

        ResourceBase::Type rtype = isModelExtension(ext) ? ResourceBase::Type::Model : isTextureExtension(ext) ? ResourceBase::Type::Texture : ResourceBase::Type::Unknown;

        UID uid = MetaFileManager::getOrCreateUID(path, rtype);
        m_pathToUID[path] = uid;
        m_uidToPath[uid] = path;

        if (isModelExtension(ext)) {
            std::string sceneName = entry.path().stem().string();
            if (!sceneExists(sceneName) || needsReimport(path)) {
                LOG("ModuleAssets: (Re)importing model %s", path.c_str());
                importAsset(path.c_str());
            }
            else {
                ModuleFileSystem* fsys = app->getFileSystem();
                int mc = 0;
                int matc = 0;
                countLibraryFiles(fsys->GetLibraryPath() + "Meshes/" + sceneName + "/", ".mesh", mc);
                countLibraryFiles(fsys->GetLibraryPath() + "Materials/" + sceneName + "/", ".mat", matc);
                registerSceneSubResources(path, sceneName, mc, matc);
            }
        }
        else if (isTextureExtension(ext)) {
            importTexture(path, ext, uid);
        }
    }
}

void ModuleAssets::importTexture(const std::string& path, const std::string& /*ext*/, UID uid) {
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string outDir = fsys->GetLibraryPath() + "Textures/";
    std::string outPath = outDir + TextureImporter::GetTextureName(path.c_str()) + ".dds";

    if (!fsys->Exists(outPath.c_str()) || needsReimport(path)) {
        LOG("ModuleAssets: Importing texture %s", path.c_str());
        fsys->CreateDir(outDir.c_str());
        if (TextureImporter::Import(path.c_str(), outPath)) {
            MetaData meta;
            MetaFileManager::load(path, meta);
            meta.lastModified = MetaFileManager::getLastModified(path);
            MetaFileManager::save(path, meta);
            app->getResources()->registerTexture(uid, outPath);
        }
    }
    else {
        app->getResources()->registerTexture(uid, outPath);
    }
}

UID ModuleAssets::importAsset(const char* filePath) {
    std::string path = normalisePath(filePath);

    ModuleFileSystem* fsys = app->getFileSystem();
    if (!fsys->Exists(path.c_str())) {
        LOG("ModuleAssets: File does not exist: %s", path.c_str());
        return 0;
    }

    {
        std::lock_guard<std::mutex> lk(m_importMutex);
        if (m_inProgressImports.count(path)) return findUID(path);
        m_inProgressImports.insert(path);
    }

    auto releaseGuard = [&] {
        std::lock_guard<std::mutex> lk(m_importMutex);
        m_inProgressImports.erase(path);
        };

    fs::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    ResourceBase::Type rtype = isModelExtension(ext) ? ResourceBase::Type::Model : isTextureExtension(ext) ? ResourceBase::Type::Texture : ResourceBase::Type::Unknown;

    UID uid = MetaFileManager::getOrCreateUID(path, rtype);
    m_pathToUID[path] = uid;
    m_uidToPath[uid] = path;

    bool ok = false;

    if (ext == ".gltf" || ext == ".glb") {
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        ok = (ext == ".gltf") ? loader.LoadASCIIFromFile(&gltfModel, &err, &warn, path.c_str()) : loader.LoadBinaryFromFile(&gltfModel, &err, &warn, path.c_str());

        if (!warn.empty()) LOG("ModuleAssets: GLTF Warning: %s", warn.c_str());
        if (!ok) {
            LOG("ModuleAssets: Failed to load GLTF: %s", err.c_str());
            releaseGuard();
            return 0;
        }

        std::string sceneName = p.stem().string();
        if (!SceneImporter::ImportFromLoadedGLTF(gltfModel, sceneName)) {
            LOG("ModuleAssets: Import failed for: %s", sceneName.c_str());
            releaseGuard();
            return 0;
        }

        int mc = 0;
        int matc = 0;
        countLibraryFiles(fsys->GetLibraryPath() + "Meshes/" + sceneName + "/", ".mesh", mc);
        countLibraryFiles(fsys->GetLibraryPath() + "Materials/" + sceneName + "/", ".mat", matc);
        registerSceneSubResources(path, sceneName, mc, matc);
        ok = true;
    }
    else if (isTextureExtension(ext)) {
        std::string outDir = fsys->GetLibraryPath() + "Textures/";
        std::string outPath = outDir + TextureImporter::GetTextureName(path.c_str()) + ".dds";
        fsys->CreateDir(outDir.c_str());
        ok = TextureImporter::Import(path.c_str(), outPath);
        if (ok) app->getResources()->registerTexture(uid, outPath);
    }
    else if (ext == ".fbx" || ext == ".stl" || ext == ".blend") {
        LOG("ModuleAssets: Format not yet supported: %s", ext.c_str());
        releaseGuard();
        return uid;
    }

    if (ok) {
        MetaData meta;
        MetaFileManager::load(path, meta);
        meta.lastModified = MetaFileManager::getLastModified(path);
        MetaFileManager::save(path, meta);
        LOG("ModuleAssets: Imported %s (uid=%llu)", path.c_str(), uid);
    }

    releaseGuard();
    return ok ? uid : 0;
}

void ModuleAssets::registerSceneSubResources(const std::string& filePath, const std::string& sceneName, int meshCount, int materialCount) {
    UID parent = findUID(filePath);
    if (parent == 0) return;

    ModuleFileSystem* fsys = app->getFileSystem();
    m_sceneNameToPath[sceneName] = filePath;

    std::string meshFolder = fsys->GetLibraryPath() + "Meshes/" + sceneName + "/";
    std::string matFolder = fsys->GetLibraryPath() + "Materials/" + sceneName + "/";

    for (int i = 0; i < meshCount; ++i) {
        UID meshUID = makeSubUID(parent, "mesh", i);
        std::string lp = meshFolder + std::to_string(i) + ".mesh";
        m_subUIDs[filePath + "|mesh|" + std::to_string(i)] = meshUID;
        m_uidToPath[meshUID] = lp;
        app->getResources()->registerMesh(meshUID, lp);
    }

    for (int i = 0; i < materialCount; ++i) {
        UID matUID = makeSubUID(parent, "mat", i);
        std::string lp = matFolder + std::to_string(i) + ".mat";
        m_subUIDs[filePath + "|mat|" + std::to_string(i)] = matUID;
        m_uidToPath[matUID] = lp;
        app->getResources()->registerMaterial(matUID, lp, 0);
    }

    LOG("ModuleAssets: Registered %d meshes, %d materials for %s", meshCount, materialCount, sceneName.c_str());
}

void ModuleAssets::deleteAsset(const std::string& assetPath) {
    std::string path = normalisePath(assetPath);
    ModuleFileSystem* fsys = app->getFileSystem();

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    UID uid = findUID(path);

    auto deleteIfExists = [&](const std::string& p) {
        if (fsys->Exists(p.c_str())) fsys->Delete(p.c_str());
        };

    if (isModelExtension(ext)) {
        std::string sceneName = fs::path(path).stem().string();
        deleteIfExists(fsys->GetLibraryPath() + "Meshes/" + sceneName);
        deleteIfExists(fsys->GetLibraryPath() + "Materials/" + sceneName);
        deleteIfExists(path + ".meta");
        deleteIfExists(path);
        for (int i = 0; ; ++i) {
            bool anyMesh = m_subUIDs.erase(path + "|mesh|" + std::to_string(i)) > 0;
            bool anyMat = m_subUIDs.erase(path + "|mat|" + std::to_string(i)) > 0;
            if (!anyMesh && !anyMat) break;
        }
        m_sceneNameToPath.erase(sceneName);
    }
    else if (isTextureExtension(ext)) {
        deleteIfExists(path + ".meta");
        deleteIfExists(path);
        if (uid != 0) {
            std::string srcPath = getPathFromUID(uid);
            if (!srcPath.empty()) deleteIfExists(srcPath + ".meta");
        }
    }

    if (uid != 0) {
        m_pathToUID.erase(path);
        m_uidToPath.erase(uid);
    }
    LOG("ModuleAssets: Asset deleted: %s", path.c_str());
}

std::string ModuleAssets::getAssetPathForScene(const std::string& sceneName) const {
    auto it = m_sceneNameToPath.find(sceneName);
    return it != m_sceneNameToPath.end() ? it->second : "";
}

UID ModuleAssets::findSubUID(const std::string& sceneAssetPath, const std::string& type, int index) const {
    std::string path = normalisePath(sceneAssetPath);
    auto it = m_subUIDs.find(path + "|" + type + "|" + std::to_string(index));
    return it != m_subUIDs.end() ? it->second : 0;
}

UID ModuleAssets::findUID(const std::string& assetPath) const {
    std::string path = normalisePath(assetPath);
    auto it = m_pathToUID.find(path);
    if (it != m_pathToUID.end()) return it->second;
    MetaData meta;
    return MetaFileManager::load(path, meta) ? meta.uid : 0;
}

std::string ModuleAssets::getPathFromUID(UID uid) const {
    auto it = m_uidToPath.find(uid);
    return it != m_uidToPath.end() ? it->second : "";
}

bool ModuleAssets::needsReimport(const std::string& assetPath) const {
    std::string path = normalisePath(assetPath);
    MetaData meta;
    return !MetaFileManager::load(path, meta) || MetaFileManager::getLastModified(path) != meta.lastModified;
}

bool ModuleAssets::sceneExists(const std::string& sceneName) const {
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string path = fsys->GetLibraryPath() + "Meshes/" + sceneName;
    return fsys->Exists(path.c_str()) && fsys->IsDirectory(path.c_str());
}

std::vector<ModuleAssets::SceneInfo> ModuleAssets::getImportedScenes() const {
    std::vector<SceneInfo> scenes;
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string meshesPath = fsys->GetLibraryPath() + "Meshes/";
    if (!fsys->Exists(meshesPath.c_str())) return scenes;

    try {
        for (const auto& entry : fs::directory_iterator(meshesPath)) {
            if (!entry.is_directory()) continue;
            SceneInfo info;
            info.name = entry.path().filename().string();
            info.path = normalisePath(entry.path().string());

            std::string assetsRoot = app->getFileSystem()->GetAssetsPath();
            info.uid = findUID(assetsRoot + "Models/" + info.name + "/" + info.name + ".gltf");

            char* buffer = nullptr;
            uint32_t size = fsys->Load((info.path + "/scene.meta").c_str(), &buffer);
            if (buffer && size >= sizeof(SceneImporter::SceneHeader)) {
                SceneImporter::SceneHeader header;
                memcpy(&header, buffer, sizeof(header));
                if (header.magic == 0x53434E45 && header.version == 1) {
                    info.meshCount = header.meshCount;
                    info.materialCount = header.materialCount;
                }
                delete[] buffer;
            }
            scenes.push_back(info);
        }
    }
    catch (const std::exception& e) {
        LOG("ModuleAssets: Error listing scenes: %s", e.what());
    }
    return scenes;
}