#include "Globals.h"
#include "ModuleFileSystem.h"

#include <filesystem>
#include <iostream>
#include <fstream>

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

bool ModuleFileSystem::Save(
    const char* path,
    const void* data,
    size_t size)
{
    if (!data || size == 0)
        return false;

    try
    {
        std::ofstream file(
            path,
            std::ios::binary | std::ios::out | std::ios::trunc
        );

        if (!file.is_open())
            return false;

        file.write(
            reinterpret_cast<const char*>(data),
            size
        );

        file.close();

        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ModuleFileSystem::Load(
    const char* path,
    std::vector<uint8_t>& outData)
{
    outData.clear();

    try
    {
        std::ifstream file(
            path,
            std::ios::binary | std::ios::in | std::ios::ate
        );

        if (!file.is_open())
            return false;

        std::streamsize size = file.tellg();

        if (size <= 0)
            return false;

        file.seekg(0, std::ios::beg);

        outData.resize((size_t)size);

        if (!file.read(
            reinterpret_cast<char*>(outData.data()),
            size))
        {
            return false;
        }

        file.close();

        return true;
    }
    catch (...)
    {
        return false;
    }
}