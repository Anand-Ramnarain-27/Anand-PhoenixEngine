#include "Globals.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "SceneImporter.h"
#include "tiny_gltf.h"
#include <filesystem>

bool ModuleAssets::init()
{
    ensureLibraryDirectories();
    return true;
}

bool ModuleAssets::cleanUp()
{
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

void ModuleAssets::importAsset(const char* filePath)
{
    ModuleFileSystem* fs = app->getFileSystem();
    if (!fs->Exists(filePath))
    {
        LOG("ModuleAssets: File does not exist: %s", filePath);
        return;
    }

    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".gltf" || ext == ".glb")
    {
        tinygltf::Model    gltfModel;
        tinygltf::TinyGLTF loader;
        std::string        err, warn;

        bool ok = (ext == ".gltf")
            ? loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath)
            : loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);

        if (!warn.empty()) LOG("ModuleAssets: GLTF Warning: %s", warn.c_str());
        if (!ok) { LOG("ModuleAssets: Failed to load GLTF: %s", err.c_str()); return; }

        std::string sceneName = path.stem().string();
        if (!SceneImporter::ImportFromLoadedGLTF(gltfModel, sceneName))
            LOG("ModuleAssets: Import failed for: %s", sceneName.c_str());
    }
    else if (ext == ".fbx")
    {
        LOG("ModuleAssets: FBX import not yet supported");
    }
    else
    {
        LOG("ModuleAssets: Unsupported format: %s", ext.c_str());
    }
}

std::vector<ModuleAssets::SceneInfo> ModuleAssets::getImportedScenes() const
{
    std::vector<SceneInfo> scenes;
    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshesPath = fs->GetLibraryPath() + "Meshes/";

    if (!fs->Exists(meshesPath.c_str())) return scenes;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(meshesPath))
        {
            if (!entry.is_directory()) continue;

            SceneInfo info;
            info.name = entry.path().filename().string();
            info.path = entry.path().string();

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
    catch (const std::exception& e)
    {
        LOG("ModuleAssets: Error reading library: %s", e.what());
    }

    return scenes;
}

bool ModuleAssets::sceneExists(const std::string& sceneName) const
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string path = fs->GetLibraryPath() + "Meshes/" + sceneName;
    return fs->Exists(path.c_str()) && fs->IsDirectory(path.c_str());
}