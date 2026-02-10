#pragma once

#include "Module.h"

class ModuleFileSystem : public Module
{
public:
    ModuleFileSystem();
    ~ModuleFileSystem() override;

    bool init() override;
    bool cleanUp() override;

    bool CreateDir(const char* path);

    const std::string& GetAssetsPath() const;
    const std::string& GetLibraryPath() const;

    // Binary IO
    bool Save(const char* path, const void* data, size_t size);
    bool Load(const char* path, std::vector<uint8_t>& outData);
private:
    void CreateProjectDirectories();

private:
    std::string assetsPath;
    std::string libraryPath;
};
