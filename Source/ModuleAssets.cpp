#include "Globals.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "SceneImporter.h"
#include "TextureImporter.h"
#include "tiny_gltf.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <filesystem>
#include <algorithm>
#include <random>
#include <chrono>

using namespace rapidjson;
namespace fs = std::filesystem;

static bool isSupportedExtension(const std::string& extLow)
{
    return extLow == ".gltf" || extLow == ".glb" ||
        extLow == ".fbx" || extLow == ".stl" ||
        extLow == ".blend" ||
        extLow == ".png" || extLow == ".jpg" ||
        extLow == ".jpeg" || extLow == ".dds" ||
        extLow == ".tga" || extLow == ".bmp" ||
        extLow == ".hdr";
}

static bool isModelExtension(const std::string& extLow)
{
    return extLow == ".gltf" || extLow == ".glb" ||
        extLow == ".fbx" || extLow == ".stl" ||
        extLow == ".blend";
}

static bool isTextureExtension(const std::string& extLow)
{
    return extLow == ".png" || extLow == ".jpg" ||
        extLow == ".jpeg" || extLow == ".dds" ||
        extLow == ".tga" || extLow == ".bmp" ||
        extLow == ".hdr";
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
    return true;
}

void ModuleAssets::ensureLibraryDirectories()
{
    ModuleFileSystem* fs = app->getFileSystem();
    fs->CreateDir("Library");
    fs->CreateDir("Library/Meshes");
    fs->CreateDir("Library/Materials");
    fs->CreateDir("Library/Textures");
    fs->CreateDir("Library/Scenes");
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

        UID uid = getOrCreateUID(path);
        m_pathToUID[path] = uid;

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
                    loadMeta(path, meta);
                    meta.lastModified = getLastModified(path);
                    saveMeta(path, meta);
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

    fs::path   path(filePath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    UID uid = getOrCreateUID(filePath);
    m_pathToUID[filePath] = uid;

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

        std::string sceneName = path.stem().string();
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
        LOG("ModuleAssets: Format not yet supported for import: %s", ext.c_str());
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
        loadMeta(filePath, meta);
        meta.lastModified = getLastModified(filePath);
        saveMeta(filePath, meta);
        LOG("ModuleAssets: Imported %s (uid=%llu)", filePath, uid);
    }

    return ok ? uid : 0;
}

UID ModuleAssets::findUID(const std::string& assetPath) const
{
    auto it = m_pathToUID.find(assetPath);
    if (it != m_pathToUID.end()) return it->second;

    MetaData meta;
    return loadMeta(assetPath, meta) ? meta.uid : 0;
}

bool ModuleAssets::needsReimport(const std::string& assetPath) const
{
    MetaData meta;
    if (!loadMeta(assetPath, meta)) return true;  
    return getLastModified(assetPath) != meta.lastModified;
}

std::vector<ModuleAssets::SceneInfo> ModuleAssets::getImportedScenes() const
{
    std::vector<SceneInfo> scenes;
    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshesPath = fs->GetLibraryPath() + "Meshes/";
    if (!fs->Exists(meshesPath.c_str())) return scenes;

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
            uint32_t size = fs->Load(metaPath.c_str(), &buffer);
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

bool ModuleAssets::sceneExists(const std::string& sceneName) const
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string path = fs->GetLibraryPath() + "Meshes/" + sceneName;
    return fs->Exists(path.c_str()) && fs->IsDirectory(path.c_str());
}

bool ModuleAssets::saveMeta(const std::string& assetPath, const MetaData& meta) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("uid", meta.uid, a);
    doc.AddMember("lastModified", meta.lastModified, a);

    StringBuffer sb;
    PrettyWriter<StringBuffer> writer(sb);
    doc.Accept(writer);

    std::string metaPath = assetPath + ".meta";
    return app->getFileSystem()->Save(metaPath.c_str(), sb.GetString(), (unsigned)sb.GetSize());
}

bool ModuleAssets::loadMeta(const std::string& assetPath, MetaData& outMeta) const
{
    std::string metaPath = assetPath + ".meta";
    char* buf = nullptr;
    unsigned size = app->getFileSystem()->Load(metaPath.c_str(), &buf);
    if (!buf || size == 0) return false;

    Document doc; doc.Parse(buf, size); delete[] buf;
    if (doc.HasParseError()) return false;

    if (doc.HasMember("uid"))          outMeta.uid = doc["uid"].GetUint64();
    if (doc.HasMember("lastModified")) outMeta.lastModified = doc["lastModified"].GetUint64();
    return true;
}

UID ModuleAssets::getOrCreateUID(const std::string& assetPath)
{
    MetaData meta;
    if (loadMeta(assetPath, meta) && meta.uid != 0)
        return meta.uid;  

    meta.uid = generateUID();
    meta.lastModified = getLastModified(assetPath);
    saveMeta(assetPath, meta);
    LOG("ModuleAssets: Created .meta for %s  (uid=%llu)", assetPath.c_str(), meta.uid);
    return meta.uid;
}

UID ModuleAssets::generateUID()
{
    static std::mt19937_64 gen(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dis(1, UINT64_MAX);
    return dis(gen);
}

uint64_t ModuleAssets::getLastModified(const std::string& filePath)
{
    try {
        auto ftime = fs::last_write_time(filePath);
        auto duration = ftime.time_since_epoch();
        return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }
    catch (...) { return 0; }
}

void ModuleAssets::requestResource(UID uid)
{
    if (uid == 0) return;
    m_refCounts[uid]++;
    LOG("ModuleAssets: requestResource uid=%llu refCount=%d",
        uid, m_refCounts[uid]);
}

void ModuleAssets::releaseResource(UID uid)
{
    if (uid == 0) return;
    auto it = m_refCounts.find(uid);
    if (it == m_refCounts.end()) return;

    it->second--;
    LOG("ModuleAssets: releaseResource uid=%llu refCount=%d",
        uid, it->second);

    if (it->second <= 0)
    {
        LOG("ModuleAssets: uid=%llu has 0 references, could unload", uid);
        m_refCounts.erase(it);
    }
}