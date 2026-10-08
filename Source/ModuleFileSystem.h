#pragma once
// Project file system: the Assets/ and Library/ roots next to the executable, plus small file helpers.

#include "Module.h"
#include <string>
#include <vector>

/// Resolves the project folders from the executable's location (so a copied editor or a shipped Player finds its
/// own Assets/ and Library/) and creates the Library/ sub-folders the importers write into.
class ModuleFileSystem : public Module {
public:
    ModuleFileSystem();
    ~ModuleFileSystem() override;

    bool init() override;
    bool cleanUp() override;

    bool CreateDir(const char* path);
    /// Reads the whole file into a new[]-allocated `*buffer` the caller deletes; returns its size, 0 on failure.
    unsigned int Load(const char* filePath, char** buffer) const;
    bool Save(const char* filePath, const void* buffer, unsigned int size, bool append = false) const;
    /// Recursive, overwriting existing files.
    bool Copy(const char* source, const char* destination);
    /// Recursive.
    bool Delete(const char* path);
    bool Exists(const char* path) const;
    bool IsDirectory(const char* path) const;

    /// Absolute, with a trailing '/'.
    const std::string& GetAssetsPath() const;
    /// Absolute, with a trailing '/'.
    const std::string& GetLibraryPath() const;
    /// Regular files directly inside `path`, optionally only those with `extension` (".json").
    std::vector<std::string> GetFilesInDirectory(const char* path, const char* extension = nullptr) const;

private:
    void CreateProjectDirectories();
    std::string m_assetsPath;
    std::string m_libraryPath;
};
