#include "Globals.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "MetaFileManager.h"
#include "SceneImporter.h"
#include "TextureImporter.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static bool isSupportedExtension(const std::string& ext)
{
    return ext == ".gltf" || ext == ".glb" ||
        ext == ".fbx" || ext == ".stl" || ext == ".blend" ||
        ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga" || ext == ".bmp" || ext == ".hdr";
}
static bool isModelExtension(const std::string& ext)
{
    return ext == ".gltf" || ext == ".glb" ||
        ext == ".fbx" || ext == ".stl" || ext == ".blend";
}
static bool isTextureExtension(const std::string& ext)
{
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga" || ext == ".bmp" || ext == ".hdr";
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
        if (ext == ".meta") continue;
        if (!isSupportedExtension(ext)) continue;

        ResourceBase::Type rtype = ResourceBase::Type::Unknown;
        if (isModelExtension(ext))        rtype = ResourceBase::Type::Model;
        else if (isTextureExtension(ext)) rtype = ResourceBase::Type::Texture;

        UID uid = MetaFileManager::getOrCreateUID(path, rtype);
        m_pathToUID[path] = uid;
        m_uidToPath[uid] = path;

        if (isModelExtension(ext))
        {
            std::string sceneName = entry.path().stem().string();
            if (!sceneExists(sceneName) || needsReimport(path))
            {
                LOG("ModuleAssets: Auto-importing model %s", path.c_str());
                importAsset(path.c_str());
            }
        }
        else if (isTextureExtension(ext))
        {
            std::string outDir = app->getFileSystem()->GetLibraryPath() + "Textures/";
            std::string texName = TextureImporter::GetTextureName(path.c_str());
            std::string outPath = outDir + texName + ".dds";

            if (!app->getFileSystem()->Exists(outPath.c_str()) || needsReimport(path))
            {
                LOG("ModuleAssets: Auto-importing texture %s", path.c_str());
                app->getFileSystem()->CreateDir(outDir.c_str());
                bool ok = TextureImporter::Import(path.c_str(), outPath.c_str());
                if (ok)
                {
                    MetaData meta;
                    MetaFileManager::load(path, meta);
                    meta.lastModified = MetaFileManager::getLastModified(path);
                    MetaFileManager::save(path, meta);
                }
            }
        }
    }
}

UID ModuleAssets::importAsset(const char* filePath)
{
    ModuleFileSystem* fsys = app->getFileSystem();
    if (!fsys->Exists(filePath))
    {
        LOG("ModuleAssets: File does not exist: %s", filePath);
        return 0;
    }

    fs::path    p(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    ResourceBase::Type rtype = isModelExtension(ext) ? ResourceBase::Type::Model
        : isTextureExtension(ext) ? ResourceBase::Type::Texture
        : ResourceBase::Type::Unknown;

    UID uid = MetaFileManager::getOrCreateUID(filePath, rtype);
    m_pathToUID[filePath] = uid;
    m_uidToPath[uid] = filePath;

    bool ok = false;

    if (ext == ".gltf" || ext == ".glb")
    {
        tinygltf::Model    gltfModel;
        tinygltf::TinyGLTF loader;
        std::string        err, warn;

        ok = (ext == ".gltf")
            ? loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath)
            : loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);

        if (!warn.empty()) LOG("ModuleAssets: GLTF Warning: %s", warn.c_str());
        if (!ok) { LOG("ModuleAssets: Failed to load GLTF: %s", err.c_str()); return 0; }

        std::string sceneName = p.stem().string();
        ok = SceneImporter::ImportFromLoadedGLTF(gltfModel, sceneName);
        if (!ok) { LOG("ModuleAssets: Import failed for: %s", sceneName.c_str()); return 0; }
    }
    else if (isTextureExtension(ext))
    {
        std::string outDir = fsys->GetLibraryPath() + "Textures/";
        std::string texName = TextureImporter::GetTextureName(filePath);
        std::string outPath = outDir + texName + ".dds";
        fsys->CreateDir(outDir.c_str());
        ok = TextureImporter::Import(filePath, outPath.c_str());
        if (!ok) LOG("ModuleAssets: Texture import failed: %s", filePath);
    }
    else if (ext == ".fbx" || ext == ".stl" || ext == ".blend")
    {
        LOG("ModuleAssets: Format not yet supported: %s", ext.c_str());
        return uid;
    }
    else
    {
        LOG("ModuleAssets: Unrecognised format: %s", ext.c_str());
        return 0;
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
    if (!MetaFileManager::load(assetPath, meta)) return true;
    return MetaFileManager::getLastModified(assetPath) != meta.lastModified;
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

            std::string sourcePath = "Assets/Models/" + info.name + "/" + info.name + ".gltf";
            info.uid = findUID(sourcePath);

            std::string metaPath = info.path + "/scene.meta";
            char* buffer = nullptr;
            uint32_t size = fsys->Load(metaPath.c_str(), &buffer);
            if (buffer && size >= sizeof(SceneImporter::SceneHeader))
            {
                SceneImporter::SceneHeader header;
                memcpy(&header, buffer, sizeof(header));
                if (header.magic == 0x53434E45 && header.version == 1)
                {
                    info.meshCount = header.meshCount;
                    info.materialCount = header.materialCount;
                }
                delete[] buffer;
            }
            scenes.push_back(info);
        }
    }
    catch (const std::exception& e) { LOG("ModuleAssets: Error: %s", e.what()); }

    return scenes;
}