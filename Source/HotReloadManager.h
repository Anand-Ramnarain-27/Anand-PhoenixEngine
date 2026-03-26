#pragma once
#include "IScript.h"
#include "IScriptFactory.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <windows.h>

struct ScriptLibrary {
    HMODULE  handle = nullptr;
    std::string dllPath;
    std::string pdbPath; 
    std::unordered_map<std::string, ScriptFactoryFn> factories;
};

class HotReloadManager {
public:
    using ReloadCallback = std::function<void(const std::string& dllName)>;

    HotReloadManager() = default;
    ~HotReloadManager();

    bool loadLibrary(const std::string& dllPath);

    bool reloadLibrary(const std::string& dllPath);

    void unloadAll();

    IScript* createScript(const std::string& className) const;

    void setReloadCallback(ReloadCallback cb) { m_reloadCb = std::move(cb); }

    bool isLoaded(const std::string& dllPath) const;

    std::vector<std::string> getRegisteredClassNames() const;

private:
    bool loadLibraryInternal(const std::string& dllPath, ScriptLibrary& out);
    std::string versionedPdbPath(const std::string& dllPath);

    std::unordered_map<std::string, ScriptLibrary> m_libraries;
    ReloadCallback m_reloadCb;
    int m_pdbVersion = 0;
};
