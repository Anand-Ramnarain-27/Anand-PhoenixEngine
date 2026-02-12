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

    bool CreateDir(const char* path);

    unsigned int Load(const char* file_path, char** buffer) const;
    bool Save(const char* file_path, const void* buffer, unsigned int size, bool append = false) const;

    bool Copy(const char* source_file_path, const char* destination_file_path);
    bool Delete(const char* file_path);
    bool CreateDirectory(const char* directory_path);
    bool Exists(const char* file_path) const;
    bool IsDirectory(const char* directory_path) const;


    const std::string& GetAssetsPath() const;
    const std::string& GetLibraryPath() const;
private:
    void CreateProjectDirectories();

private:
    std::string assetsPath;
    std::string libraryPath;
};
