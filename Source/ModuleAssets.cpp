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

static bool isModelExtension(const std::string& ext) { return ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".stl" || ext == ".blend"; }
static bool isTextureExtension(const std::string& ext) { return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".bmp" || ext == ".hdr"; }
static bool isSupportedExtension(const std::string& ext) { return isModelExtension(ext) || isTextureExtension(ext); }

static UID makeSubUID(UID parentUID, const std::string& type, int index)
{
    std::string key = std::to_string(parentUID) + "|" + type + "|" + std::to_string(index);
    UID hash = 14695981039346656037ULL;
    for (char c : key) { hash ^= (uint8_t)c; hash *= 1099511628211ULL; }
    return hash ? hash : 1;
}

bool ModuleAssets::init()
{
    ensureLibraryDirectories();
    refreshAssets();
    return true;
}

bool ModuleAssets::cleanUp()
{
    m_pathToUID.clear();
    m_uidToPath.clear();
    m_subUIDs.clear();
    m_sceneNameToPath.clear();
    return true;
}

void ModuleAssets::ensureLibraryDirectories()
{
    ModuleFileSystem* fsys = app->getFileSystem();
    fsys->CreateDir("Library");
    fsys->CreateDir("Library/Meshes");
    fsys->CreateDir("Library/Materials");
    fsys->CreateDir("Library/Textures");
    fsys->CreateDir("Library/Scenes");
}

void ModuleAssets::countLibraryFiles(const std::string& folder, const std::string& ext, int& count) const
{
    count = 0;
    while (app->getFileSystem()->Exists((folder + std::to_string(count) + ext).c_str())) ++count;
}

void ModuleAssets::importTexture(const std::string& path, const std::string& ext, UID uid)
{
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string outDir = fsys->GetLibraryPath() + "Textures/";
    std::string outPath = outDir + TextureImporter::GetTextureName(path.c_str()) + ".dds";

    if (!fsys->Exists(outPath.c_str()) || needsReimport(path))
    {
        LOG("ModuleAssets: Auto-importing texture %s", path.c_str());
        fsys->CreateDir(outDir.c_str());
        if (TextureImporter::Import(path.c_str(), outPath.c_str()))
        {
            MetaData meta;
            MetaFileManager::load(path, meta);
            meta.lastModified = MetaFileManager::getLastModified(path);
            MetaFileManager::save(path, meta);
            app->getResources()->registerTexture(uid, outPath);
        }
    }
    else app->getResources()->registerTexture(uid, outPath);
}

void ModuleAssets::refreshAssets()
{
    std::string assetsRoot = app->getFileSystem()->GetAssetsPath();
    if (!fs::exists(assetsRoot)) return;

    for (const auto& entry : fs::recursive_directory_iterator(assetsRoot))
    {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".meta" || !isSupportedExtension(ext)) continue;

        ResourceBase::Type rtype = isModelExtension(ext) ? ResourceBase::Type::Model : isTextureExtension(ext) ? ResourceBase::Type::Texture : ResourceBase::Type::Unknown;
        UID uid = MetaFileManager::getOrCreateUID(path, rtype);
        m_pathToUID[path] = uid;
        m_uidToPath[uid] = path;

        if (isModelExtension(ext))
        {
            std::string sceneName = entry.path().stem().string();
            if (!sceneExists(sceneName) || needsReimport(path)) { LOG("ModuleAssets: Auto-importing model %s", path.c_str()); importAsset(path.c_str()); }
            else
            {
                ModuleFileSystem* fsys = app->getFileSystem();
                int mc = 0, matc = 0;
                countLibraryFiles(fsys->GetLibraryPath() + "Meshes/" + sceneName + "/", ".mesh", mc);
                countLibraryFiles(fsys->GetLibraryPath() + "Materials/" + sceneName + "/", ".mat", matc);
                registerSceneSubResources(path, sceneName, mc, matc);
            }
        }
        else if (isTextureExtension(ext)) importTexture(path, ext, uid);
    }
}

UID ModuleAssets::importAsset(const char* filePath)
{
    ModuleFileSystem* fsys = app->getFileSystem();
    if (!fsys->Exists(filePath)) { LOG("ModuleAssets: File does not exist: %s", filePath); return 0; }

    fs::path p(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    ResourceBase::Type rtype = isModelExtension(ext) ? ResourceBase::Type::Model : isTextureExtension(ext) ? ResourceBase::Type::Texture : ResourceBase::Type::Unknown;
    UID uid = MetaFileManager::getOrCreateUID(filePath, rtype);
    m_pathToUID[filePath] = uid;
    m_uidToPath[uid] = filePath;

    bool ok = false;

    if (ext == ".gltf" || ext == ".glb")
    {
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        ok = (ext == ".gltf") ? loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath) : loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);
        if (!warn.empty()) LOG("ModuleAssets: GLTF Warning: %s", warn.c_str());
        if (!ok) { LOG("ModuleAssets: Failed to load GLTF: %s", err.c_str()); return 0; }

        std::string sceneName = p.stem().string();
        if (!SceneImporter::ImportFromLoadedGLTF(gltfModel, sceneName)) { LOG("ModuleAssets: Import failed for: %s", sceneName.c_str()); return 0; }

        int mc = 0, matc = 0;
        countLibraryFiles(fsys->GetLibraryPath() + "Meshes/" + sceneName + "/", ".mesh", mc);
        countLibraryFiles(fsys->GetLibraryPath() + "Materials/" + sceneName + "/", ".mat", matc);
        registerSceneSubResources(filePath, sceneName, mc, matc);
        ok = true;
    }
    else if (isTextureExtension(ext))
    {
        std::string outDir = fsys->GetLibraryPath() + "Textures/";
        std::string outPath = outDir + TextureImporter::GetTextureName(filePath) + ".dds";
        fsys->CreateDir(outDir.c_str());
        ok = TextureImporter::Import(filePath, outPath.c_str());
        if (ok) app->getResources()->registerTexture(uid, outPath);
    }
    else if (ext == ".fbx" || ext == ".stl" || ext == ".blend")
    {
        LOG("ModuleAssets: Format not yet supported: %s", ext.c_str());
        return uid;
    }

    if (ok)
    {
        MetaData meta;
        MetaFileManager::load(filePath, meta);
        meta.lastModified = MetaFileManager::getLastModified(filePath);
        MetaFileManager::save(filePath, meta);
        LOG("ModuleAssets: Imported %s (uid=%llu)", filePath, uid);
    }

    return ok ? uid : 0;
}

void ModuleAssets::registerSceneSubResources(const std::string& filePath, const std::string& sceneName, int meshCount, int materialCount)
{
    UID parent = findUID(filePath);
    if (parent == 0) return;

    ModuleFileSystem* fsys = app->getFileSystem();
    m_sceneNameToPath[sceneName] = filePath;
    std::string meshFolder = fsys->GetLibraryPath() + "Meshes/" + sceneName + "/";
    std::string matFolder = fsys->GetLibraryPath() + "Materials/" + sceneName + "/";

    for (int i = 0; i < meshCount; ++i)
    {
        UID meshUID = makeSubUID(parent, "mesh", i);
        std::string libPath = meshFolder + std::to_string(i) + ".mesh";
        m_subUIDs[filePath + "|mesh|" + std::to_string(i)] = meshUID;
        m_uidToPath[meshUID] = libPath;
        app->getResources()->registerMesh(meshUID, libPath);
    }

    for (int i = 0; i < materialCount; ++i)
    {
        UID matUID = makeSubUID(parent, "mat", i);
        std::string libPath = matFolder + std::to_string(i) + ".mat";
        m_subUIDs[filePath + "|mat|" + std::to_string(i)] = matUID;
        m_uidToPath[matUID] = libPath;
        app->getResources()->registerMaterial(matUID, libPath, 0);
    }

    LOG("ModuleAssets: Registered %d meshes, %d materials for %s", meshCount, materialCount, sceneName.c_str());
}

void ModuleAssets::deleteAsset(const std::string& assetPath)
{
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string normPath = assetPath;
    for (char& c : normPath) if (c == '\\') c = '/';

    std::string ext = fs::path(normPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    UID uid = findUID(normPath);

    auto deleteIfExists = [&](const std::string& path) { if (fsys->Exists(path.c_str())) fsys->Delete(path.c_str()); };

    if (isModelExtension(ext))
    {
        std::string sceneName = fs::path(normPath).stem().string();
        deleteIfExists(fsys->GetLibraryPath() + "Meshes/" + sceneName);
        deleteIfExists(fsys->GetLibraryPath() + "Materials/" + sceneName);
        deleteIfExists(normPath + ".meta");
        deleteIfExists(normPath);

        for (int i = 0; ; ++i)
        {
            bool anyMesh = m_subUIDs.erase(normPath + "|mesh|" + std::to_string(i)) > 0;
            bool anyMat = m_subUIDs.erase(normPath + "|mat|" + std::to_string(i)) > 0;
            if (!anyMesh && !anyMat) break;
        }
        m_sceneNameToPath.erase(sceneName);
    }
    else if (isTextureExtension(ext))
    {
        deleteIfExists(normPath + ".meta");
        deleteIfExists(normPath);
        if (uid != 0)
        {
            std::string srcPath = getPathFromUID(uid);
            if (!srcPath.empty()) deleteIfExists(srcPath + ".meta");
        }
    }

    if (uid != 0) { m_pathToUID.erase(normPath); m_uidToPath.erase(uid); }
    LOG("ModuleAssets: Asset deleted: %s", normPath.c_str());
}

std::string ModuleAssets::getAssetPathForScene(const std::string& sceneName) const
{
    auto it = m_sceneNameToPath.find(sceneName);
    return it != m_sceneNameToPath.end() ? it->second : "";
}

UID ModuleAssets::findSubUID(const std::string& sceneAssetPath, const std::string& type, int index) const
{
    auto it = m_subUIDs.find(sceneAssetPath + "|" + type + "|" + std::to_string(index));
    return it != m_subUIDs.end() ? it->second : 0;
}

UID ModuleAssets::findUID(const std::string& assetPath) const
{
    auto it = m_pathToUID.find(assetPath);
    if (it != m_pathToUID.end()) return it->second;
    MetaData meta;
    return MetaFileManager::load(assetPath, meta) ? meta.uid : 0;
}

std::string ModuleAssets::getPathFromUID(UID uid) const
{
    auto it = m_uidToPath.find(uid);
    return it != m_uidToPath.end() ? it->second : "";
}

bool ModuleAssets::needsReimport(const std::string& assetPath) const
{
    MetaData meta;
    return !MetaFileManager::load(assetPath, meta) || MetaFileManager::getLastModified(assetPath) != meta.lastModified;
}

bool ModuleAssets::sceneExists(const std::string& sceneName) const
{
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string path = fsys->GetLibraryPath() + "Meshes/" + sceneName;
    return fsys->Exists(path.c_str()) && fsys->IsDirectory(path.c_str());
}

std::vector<ModuleAssets::SceneInfo> ModuleAssets::getImportedScenes() const
{
    std::vector<SceneInfo> scenes;
    ModuleFileSystem* fsys = app->getFileSystem();
    std::string meshesPath = fsys->GetLibraryPath() + "Meshes/";
    if (!fsys->Exists(meshesPath.c_str())) return scenes;

    try
    {
        for (const auto& entry : fs::directory_iterator(meshesPath))
        {
            if (!entry.is_directory()) continue;
            SceneInfo info;
            info.name = entry.path().filename().string();
            info.path = entry.path().string();
            info.uid = findUID("Assets/Models/" + info.name + "/" + info.name + ".gltf");

            char* buffer = nullptr;
            uint32_t size = fsys->Load((info.path + "/scene.meta").c_str(), &buffer);
            if (buffer && size >= sizeof(SceneImporter::SceneHeader))
            {
                SceneImporter::SceneHeader header;
                memcpy(&header, buffer, sizeof(header));
                if (header.magic == 0x53434E45 && header.version == 1) { info.meshCount = header.meshCount; info.materialCount = header.materialCount; }
                delete[] buffer;
            }
            scenes.push_back(info);
        }
    }
    catch (const std::exception& e) { LOG("ModuleAssets: Error: %s", e.what()); }

    return scenes;
}