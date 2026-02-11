#include "Globals.h"
#include "ModuleFileSystem.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

ModuleFileSystem::ModuleFileSystem() = default;
ModuleFileSystem::~ModuleFileSystem() = default;

bool ModuleFileSystem::init()
{
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
        std::cerr << "[FileSystem] CreateDir error: "
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

unsigned int ModuleFileSystem::Load(const char* path, char** buffer) const
{
    if (!Exists(path))
        return 0;

    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open())
        return 0;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    *buffer = new char[size];

    if (!file.read(*buffer, size))
    {
        delete[] * buffer;
        *buffer = nullptr;
        return 0;
    }

    return (unsigned int)size;
}

bool ModuleFileSystem::Exists(const char* path) const
{
    return fs::exists(path);
}

bool ModuleFileSystem::IsDirectory(const char* path) const
{
    return fs::is_directory(path);
}

bool ModuleFileSystem::Delete(const char* path)
{
    try
    {
        return fs::remove_all(path) > 0;
    }
    catch (...)
    {
        return false;
    }
}

bool ModuleFileSystem::Copy(const char* source, const char* destination)
{
    try
    {
        fs::copy(source, destination,fs::copy_options::overwrite_existing | fs::copy_options::recursive);
        return true;
    }
    catch (...)
    {
        return false;
    }
}


