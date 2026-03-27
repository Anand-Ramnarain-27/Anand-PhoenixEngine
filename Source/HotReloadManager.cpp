#include "Globals.h"    
#include "HotReloadManager.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static std::string norm(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}

HotReloadManager::~HotReloadManager() { unloadAll(); }
 
bool HotReloadManager::loadLibrary(const std::string& dllPath) {
    std::string key = norm(dllPath);
    if (m_libraries.count(key)) return true;   
    ScriptLibrary lib;
    if (!loadLibraryInternal(key, lib)) return false;
    m_libraries[key] = std::move(lib);
    return true;
}

bool HotReloadManager::reloadLibrary(const std::string& dllPath) {
    std::string key = norm(dllPath);
    auto it = m_libraries.find(key);
    if (it != m_libraries.end()) {
        FreeLibrary(it->second.handle);  
        m_libraries.erase(it);
    }
    ScriptLibrary lib;
    if (!loadLibraryInternal(key, lib)) {
        LOG("[HotReload] FAILED to reload: %s", key.c_str());
        return false;
    }
    m_libraries[key] = std::move(lib);
    LOG("[HotReload] Reloaded: %s", key.c_str());
    if (m_reloadCb) m_reloadCb(key);   
    return true;
}

void HotReloadManager::unloadAll() {
    for (auto& [k, lib] : m_libraries)
        if (lib.handle) FreeLibrary(lib.handle);
    m_libraries.clear();
}

IScript* HotReloadManager::createScript(const std::string& className) const {
    for (const auto& [k, lib] : m_libraries) {
        auto it = lib.factories.find(className);
        if (it != lib.factories.end()) return it->second(); 
    }
    LOG("[HotReload] Unknown script class: '%s'", className.c_str());
    return nullptr;
}

std::vector<std::string> HotReloadManager::getRegisteredClassNames() const {
    std::vector<std::string> names;
    for (const auto& [k, lib] : m_libraries)
        for (const auto& [name, fn] : lib.factories)
            names.push_back(name);
    return names;
}

bool HotReloadManager::isLoaded(const std::string& dllPath) const {
    return m_libraries.count(norm(dllPath)) > 0;
}

bool HotReloadManager::loadLibraryInternal(const std::string& dllPath, ScriptLibrary& out) {
    std::string pdbSrc = fs::path(dllPath).replace_extension(".pdb").string();
    out.pdbPath = versionedPdbPath(dllPath);
    if (fs::exists(pdbSrc))
        fs::copy_file(pdbSrc, out.pdbPath, fs::copy_options::overwrite_existing);

    out.handle = LoadLibraryA(dllPath.c_str());
    if (!out.handle) {
        LOG("[HotReload] LoadLibraryA failed for '%s' (GetLastError=%lu)",
            dllPath.c_str(), GetLastError());
        return false;
    }
    out.dllPath = dllPath;

    auto base = reinterpret_cast<const BYTE*>(out.handle);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto& expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir.VirtualAddress == 0) return true;  

    auto exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
        base + expDir.VirtualAddress);
    auto names = reinterpret_cast<const DWORD*>(base + exp->AddressOfNames);

    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* sym = reinterpret_cast<const char*>(base + names[i]);
        if (strncmp(sym, "Create_", 7) == 0) {
            auto fn = reinterpret_cast<ScriptFactoryFn>(GetProcAddress(out.handle, sym));
            if (fn) {
                std::string className = sym + 7;  
                out.factories[className] = fn;
                LOG("[HotReload] Registered script: '%s'", className.c_str());
            }
        }
    }

    static constexpr int kMaxPdbs = 5;
    if (m_pdbVersion > kMaxPdbs) {
        fs::path p(dllPath);
        std::string dir = (fs::path(app->getFileSystem()->GetLibraryPath()) / "Scripts").string();
        std::string stem = p.stem().string();
        std::string old = dir + "/" + stem + "_v" +
            std::to_string(m_pdbVersion - kMaxPdbs) + ".pdb";
        if (fs::exists(old)) fs::remove(old);
    }
    return true;
}

std::string HotReloadManager::versionedPdbPath(const std::string& dllPath) {
    fs::path p(dllPath);
    std::string stem = p.stem().string();
    std::string dir = (fs::path(app->getFileSystem()->GetLibraryPath()) / "Scripts").string();
    app->getFileSystem()->CreateDir(dir.c_str());
    return dir + "/" + stem + "_v" + std::to_string(++m_pdbVersion) + ".pdb";
}
