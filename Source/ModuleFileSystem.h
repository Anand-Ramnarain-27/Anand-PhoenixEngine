#pragma once

#include "Module.h"

#include <string>
#include <vector>
#include <cstdint>

class ModuleFileSystem : public Module
{
public:
    ModuleFileSystem();
    ~ModuleFileSystem() override;

    bool init() override;
    bool cleanUp() override;

    // Directories
    bool CreateDir(const char* path);

    const std::string& GetAssetsPath() const;
    const std::string& GetLibraryPath() const;

    // Binary I/O
    bool Save(const char* path, const void* data, size_t size);
    bool Load(const char* path, std::vector<uint8_t>& outData);

private:
    void CreateProjectDirectories();

private:
    std::string assetsPath;
    std::string libraryPath;
};
