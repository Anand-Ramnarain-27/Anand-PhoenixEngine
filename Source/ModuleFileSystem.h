#pragma once
#include "Module.h"
#include <string>
#include <vector>

class ModuleFileSystem : public Module {
public:
    ModuleFileSystem();
    ~ModuleFileSystem() override;

    bool init() override;
    bool cleanUp() override;

    bool CreateDir(const char* path);
    unsigned int Load(const char* filePath, char** buffer) const;
    bool Save(const char* filePath, const void* buffer, unsigned int size, bool append = false) const;
    bool Copy(const char* source, const char* destination);
    bool Delete(const char* path);
    bool Exists(const char* path) const;
    bool IsDirectory(const char* path) const;

    const std::string& GetAssetsPath() const;
    const std::string& GetLibraryPath() const;
    std::vector<std::string> GetFilesInDirectory(const char* path, const char* extension = nullptr) const;

private:
    void CreateProjectDirectories();
    std::string assetsPath;
    std::string libraryPath;
};