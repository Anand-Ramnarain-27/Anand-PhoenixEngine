#include "Globals.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "SceneImporter.h"

// Only include the header, not the implementation
// (Implementation is already in MeshImporter.cpp or MaterialImporter.cpp)
#include "tiny_gltf.h"

#include <filesystem>
#include <cstring>

ModuleAssets::ModuleAssets() = default;
ModuleAssets::~ModuleAssets() = default;

bool ModuleAssets::init()
{
    LOG("ModuleAssets: Initializing");

    ensureLibraryDirectories();

    LOG("ModuleAssets: Initialized successfully");
    return true;
}

bool ModuleAssets::cleanUp()
{
    LOG("ModuleAssets: Cleanup");
    return true;
}

void ModuleAssets::ensureLibraryDirectories()
{
    ModuleFileSystem* fs = app->getFileSystem();

    // Create library subdirectories if they don't exist
    fs->CreateDir("Library");
    fs->CreateDir("Library/Meshes");
    fs->CreateDir("Library/Materials");
    fs->CreateDir("Library/Textures");
    fs->CreateDir("Library/Scenes");

    LOG("ModuleAssets: Library directories verified");
}

void ModuleAssets::importAsset(const char* filePath)
{
    LOG("ModuleAssets: Importing asset: %s", filePath);

    ModuleFileSystem* fs = app->getFileSystem();

    // Check if file exists
    if (!fs->Exists(filePath))
    {
        LOG("ModuleAssets: ERROR - File does not exist: %s", filePath);
        return;
    }

    // Determine file type from extension
    std::filesystem::path path(filePath);
    std::string extension = path.extension().string();

    // Convert to lowercase for comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == ".gltf" || extension == ".glb")
    {
        // Load GLTF file
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool success = false;
        if (extension == ".gltf")
        {
            success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
        }
        else if (extension == ".glb")
        {
            success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);
        }

        // Log warnings
        if (!warn.empty())
        {
            LOG("ModuleAssets: GLTF Warning: %s", warn.c_str());
        }

        // Check for errors
        if (!success)
        {
            LOG("ModuleAssets: ERROR - Failed to load GLTF: %s", err.c_str());
            return;
        }

        // Extract scene name from file path
        std::string sceneName = path.stem().string();

        LOG("ModuleAssets: Importing scene: %s", sceneName.c_str());
        LOG("ModuleAssets:   Meshes: %d", (int)gltfModel.meshes.size());
        LOG("ModuleAssets:   Materials: %d", (int)gltfModel.materials.size());
        LOG("ModuleAssets:   Textures: %d", (int)gltfModel.textures.size());

        // Import to custom format using SceneImporter
        if (SceneImporter::ImportFromLoadedGLTF(gltfModel, sceneName))
        {
            LOG("ModuleAssets: SUCCESS - Imported: %s", sceneName.c_str());
            LOG("ModuleAssets:   Location: Library/Meshes/%s/", sceneName.c_str());
        }
        else
        {
            LOG("ModuleAssets: ERROR - Import failed for: %s", sceneName.c_str());
        }
    }
    else if (extension == ".fbx")
    {
        LOG("ModuleAssets: FBX import not yet supported");
        // TODO: Add FBX support later if needed
    }
    else
    {
        LOG("ModuleAssets: Unsupported file format: %s", extension.c_str());
    }
}

std::vector<ModuleAssets::SceneInfo> ModuleAssets::getImportedScenes() const
{
    std::vector<SceneInfo> scenes;

    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshesPath = fs->GetLibraryPath() + "Meshes/";

    if (!fs->Exists(meshesPath.c_str()))
    {
        return scenes; // Empty list
    }

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(meshesPath))
        {
            if (entry.is_directory())
            {
                std::string sceneName = entry.path().filename().string();
                std::string metaPath = entry.path().string() + "/scene.meta";

                SceneInfo info;
                info.name = sceneName;
                info.path = entry.path().string();
                info.meshCount = 0;
                info.materialCount = 0;

                // Try to load metadata
                if (fs->Exists(metaPath.c_str()))
                {
                    char* buffer = nullptr;
                    uint32_t size = fs->Load(metaPath.c_str(), &buffer);

                    if (buffer && size >= sizeof(SceneImporter::SceneHeader))
                    {
                        SceneImporter::SceneHeader header;
                        memcpy(&header, buffer, sizeof(SceneImporter::SceneHeader));

                        if (header.magic == 0x53434E45 && header.version == 1)
                        {
                            info.meshCount = header.meshCount;
                            info.materialCount = header.materialCount;
                        }

                        delete[] buffer;
                    }
                }

                scenes.push_back(info);
            }
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
    std::string scenePath = fs->GetLibraryPath() + "Meshes/" + sceneName;

    return fs->Exists(scenePath.c_str()) && fs->IsDirectory(scenePath.c_str());
}