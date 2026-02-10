#pragma once

#include "Module.h"

#include <string>

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

private:
    void CreateProjectDirectories();

private:
    std::string assetsPath;
    std::string libraryPath;
};
