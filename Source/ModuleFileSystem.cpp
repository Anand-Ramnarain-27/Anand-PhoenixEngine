#include "Globals.h"
#include "ModuleFileSystem.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

ModuleFileSystem::ModuleFileSystem() = default;
ModuleFileSystem::~ModuleFileSystem() = default;

bool ModuleFileSystem::init(){
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string baseDir = std::filesystem::path(exePath).parent_path().string() + "/";

    m_assetsPath = baseDir + "Assets/";
    m_libraryPath = baseDir + "Library/";

    CreateProjectDirectories();
    PHX_LOG(Core, Verbose, "FileSystem: project root %s", baseDir.c_str());
    return true;
}

bool ModuleFileSystem::cleanUp(){
    return true;
}

void ModuleFileSystem::CreateProjectDirectories(){
    CreateDir(m_assetsPath.c_str());
    CreateDir(m_libraryPath.c_str());
    CreateDir((m_libraryPath + "Meshes").c_str());
    CreateDir((m_libraryPath + "Materials").c_str());
    CreateDir((m_libraryPath + "Textures").c_str());
    CreateDir((m_libraryPath + "Scenes").c_str());
    CreateDir((m_libraryPath + "Prefabs").c_str());
    CreateDir((m_libraryPath + "metadata").c_str());
}

bool ModuleFileSystem::CreateDir(const char* path){
    try { return fs::exists(path) || fs::create_directories(path); }
    catch (const fs::filesystem_error& e){
        PHX_LOG(Core, Error, "FileSystem: can't create '%s': %s", path, e.what());
        return false;
    }
}

const std::string& ModuleFileSystem::GetAssetsPath() const { return m_assetsPath; }
const std::string& ModuleFileSystem::GetLibraryPath() const { return m_libraryPath; }

unsigned int ModuleFileSystem::Load(const char* path, char** buffer) const{
    if (!Exists(path)) return 0;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return 0;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    *buffer = new char[size];
    if (!file.read(*buffer, size)){ delete[] *buffer; *buffer = nullptr; return 0; }
    return (unsigned int)size;
}

bool ModuleFileSystem::Save(const char* path, const void* buffer, unsigned int size, bool append) const{
    std::ios::openmode mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    std::ofstream file(path, mode);
    if (!file.is_open()) return false;
    file.write((const char*)buffer, size);
    return true;
}

bool ModuleFileSystem::Exists(const char* path) const { return fs::exists(path); }
bool ModuleFileSystem::IsDirectory(const char* path) const { return fs::is_directory(path); }

bool ModuleFileSystem::Delete(const char* path){
    try { return fs::remove_all(path) > 0; }
    catch (...){ return false; }
}

bool ModuleFileSystem::Copy(const char* source, const char* destination){
    try { fs::copy(source, destination, fs::copy_options::overwrite_existing | fs::copy_options::recursive); return true; }
    catch (...){ return false; }
}

std::vector<std::string> ModuleFileSystem::GetFilesInDirectory(const char* path, const char* ext) const{
    std::vector<std::string> results;
    if (!fs::exists(path)) return results;
    for (const auto& entry : fs::directory_iterator(path)){
        if (!entry.is_regular_file()) continue;
        if (ext && entry.path().extension() != ext) continue;
        results.push_back(entry.path().string());
    }
    return results;
}
