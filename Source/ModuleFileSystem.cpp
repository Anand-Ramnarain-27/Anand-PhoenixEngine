#include "Globals.h"
#include "ModuleFileSystem.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

ModuleFileSystem::ModuleFileSystem()
{
}

ModuleFileSystem::~ModuleFileSystem()
{
}

bool ModuleFileSystem::init()
{
    // Base project directories
    assetsPath = "Assets/";
    libraryPath = "Library/";

    CreateProjectDirectories();

    std::cout << "[FileSystem] Initialized\n";

    return true;
}

bool ModuleFileSystem::cleanUp()
{
    std::cout << "[FileSystem] Shutdown\n";
    return true;
}

void ModuleFileSystem::CreateProjectDirectories()
{
    CreateDir(assetsPath.c_str());
    CreateDir(libraryPath.c_str());

    CreateDir("Library/Meshes");
    CreateDir("Library/Textures");
    CreateDir("Library/Scenes");
}

bool ModuleFileSystem::CreateDir(const char* path)
{
    try
    {
        if (fs::exists(path))
            return true;

        return fs::create_directories(path);
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "[FileSystem] CreateDir failed: "
            << e.what() << std::endl;

        return false;
    }
}

const std::string& ModuleFileSystem::GetAssetsPath() const
{
    return assetsPath;
}

const std::string& ModuleFileSystem::GetLibraryPath() const
{
    return libraryPath;
}
