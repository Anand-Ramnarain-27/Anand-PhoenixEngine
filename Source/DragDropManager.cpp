#include "Globals.h"
#include "DragDropManager.h"
#include "Application.h"
#include "ModuleAssets.h"
#include "ModuleFileSystem.h"

#include <ole2.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Extension tables
// ---------------------------------------------------------------------------
static const char* kModelExts[]   = { ".gltf", ".glb", ".fbx", ".obj", ".stl", ".blend", nullptr };
static const char* kTextureExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".dds", ".bmp", ".hdr", nullptr };
static const char* kAnimExts[]    = { ".anim", nullptr };
static const char* kMatExts[]     = { ".mat",  nullptr };

bool DragDropManager::IsSupportedExtension(const std::string& ext) {
    for (auto** p = kModelExts;   *p; ++p) if (ext == *p) return true;
    for (auto** p = kTextureExts; *p; ++p) if (ext == *p) return true;
    for (auto** p = kAnimExts;    *p; ++p) if (ext == *p) return true;
    for (auto** p = kMatExts;     *p; ++p) if (ext == *p) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
DragDropManager& DragDropManager::Get() {
    static DragDropManager s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
// Dragging state (set/read from any thread — atomic)
// ---------------------------------------------------------------------------
void DragDropManager::SetDragging(bool dragging) { m_isDragging.store(dragging); }
bool DragDropManager::IsDragging()          const { return m_isDragging.load(); }

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void DragDropManager::SetRefreshCallback(std::function<void()> cb) {
    m_refreshCallback = std::move(cb);
}

// ---------------------------------------------------------------------------
// QueueItems — primary entry point (main thread)
// ---------------------------------------------------------------------------
void DragDropManager::QueueItems(std::vector<DropItem> items) {
    if (items.empty()) return;

    // Filter individual files to supported extensions only.
    // Folder items are kept as-is; the worker decides what to import from them.
    std::vector<DropItem> filtered;
    for (auto& item : items) {
        if (item.isFolder) {
            filtered.push_back(std::move(item));
        } else {
            std::string ext = item.path.extension().string();
            for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (IsSupportedExtension(ext)) {
                filtered.push_back(std::move(item));
            } else {
                LOG("DragDrop: Skipping unsupported extension for '%s'",
                    item.path.filename().string().c_str());
            }
        }
    }
    if (filtered.empty()) return;

    m_allDone.store(false);
    m_showingComplete = false;
    {
        std::lock_guard<std::mutex> lk(m_progressMutex);
        m_progress        = {};
        m_progress.total  = static_cast<int>(filtered.size());
        m_progress.active = true;
    }

    {
        std::lock_guard<std::mutex> lk(m_workerMutex);
        if (m_worker.joinable()) m_worker.join();
        m_worker = std::thread(&DragDropManager::workerProc, this, std::move(filtered));
    }
}

// Convenience wrapper for callers that only deal with individual file paths.
void DragDropManager::QueueFiles(const std::vector<fs::path>& paths) {
    std::vector<DropItem> items;
    items.reserve(paths.size());
    for (const auto& p : paths) items.push_back({ p, false });
    QueueItems(std::move(items));
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------
static void importSingleFile(const fs::path& srcPath,
    const std::string& assetsRoot,
    ModuleFileSystem* fsys)
{
    std::string ext = srcPath.extension().string();
    for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    std::string subDir;
    for (auto** p = kModelExts; *p; ++p) if (ext == *p) { subDir = "Models/";     break; }
    for (auto** p = kTextureExts; *p; ++p) if (ext == *p) { subDir = "Textures/";   break; }
    for (auto** p = kAnimExts; *p; ++p) if (ext == *p) { subDir = "Animations/"; break; }
    for (auto** p = kMatExts; *p; ++p) if (ext == *p) { subDir = "Materials/";  break; }

    std::string destDir = assetsRoot + subDir;
    fsys->CreateDir(destDir.c_str());

    std::string destPath = destDir + srcPath.filename().string();

    bool alreadyInAssets = (srcPath.string().rfind(assetsRoot, 0) == 0);
    if (!alreadyInAssets) {
        if (!fsys->Copy(srcPath.string().c_str(), destPath.c_str())) {
            LOG("DragDrop: Failed to copy '%s' -> '%s'",
                srcPath.string().c_str(), destPath.c_str());
            return;
        }
        LOG("DragDrop: Copied '%s' -> '%s'", srcPath.string().c_str(), destPath.c_str());
    }
    else {
        destPath = srcPath.string();
    }

    UID uid = app->getAssets()->importAsset(destPath.c_str());
    if (uid != 0)
    {
        LOG("DragDrop: Imported '%s' (uid=%llu)", destPath.c_str(), uid);
    }
    else{
        LOG("DragDrop: importAsset returned 0 for '%s'", destPath.c_str());
        }
}

void DragDropManager::workerProc(std::vector<DropItem> items) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!app || !app->getAssets() || !app->getFileSystem()) {
        {
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress.active = false;
        }
        CoUninitialize();
        m_allDone.store(true);
        return;
    }

    ModuleFileSystem*  fsys       = app->getFileSystem();
    const std::string  assetsRoot = fsys->GetAssetsPath();

    for (int idx = 0; idx < static_cast<int>(items.size()); ++idx) {
        const DropItem& item = items[idx];

        {
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress.current     = idx;
            m_progress.currentFile = item.path.filename().string();
        }

        if (item.isFolder) {
            // ----------------------------------------------------------------
            // FOLDER DROP: copy the entire directory tree intact so that
            // .gltf/.fbx files keep their relative .bin and texture references.
            // Only model files in the copy are then passed to importAsset.
            // ----------------------------------------------------------------
            std::string folderName = item.path.filename().string();
            fs::path    destRoot   = fs::path(assetsRoot + "Models/") / folderName;

            bool alreadyInAssets = (item.path.string().rfind(assetsRoot, 0) == 0);
            if (!alreadyInAssets) {
                try {
                    fs::copy(item.path, destRoot,
                             fs::copy_options::recursive |
                             fs::copy_options::overwrite_existing);
                    LOG("DragDrop: Copied folder '%s' -> '%s'",
                        item.path.string().c_str(), destRoot.string().c_str());
                } catch (const std::exception& ex) {
                    LOG("DragDrop: Failed to copy folder '%s': %s",
                        item.path.string().c_str(), ex.what());
                    continue;
                }
            } else {
                destRoot = item.path;
            }

            // Import model files found anywhere in the copied folder
            try {
                for (const auto& entry : fs::recursive_directory_iterator(destRoot)) {
                    if (!entry.is_regular_file()) continue;
                    std::string ext = entry.path().extension().string();
                    for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                    bool isModel = false;
                    for (auto** p = kModelExts; *p; ++p) if (ext == *p) { isModel = true; break; }
                    if (!isModel) continue;

                    UID uid = app->getAssets()->importAsset(entry.path().string().c_str());
                    if (uid != 0)
                    {
                        LOG("DragDrop: Imported '%s' from folder (uid=%llu)",
                            entry.path().filename().string().c_str(), uid);
                    }
                    else
                    {
                        LOG("DragDrop: importAsset returned 0 for '%s'",
                            entry.path().filename().string().c_str());
                    }
                }
            } catch (...) {
                LOG("DragDrop: Error walking copied folder '%s'",
                    destRoot.string().c_str());
            }
        } else {
            // ----------------------------------------------------------------
            // INDIVIDUAL FILE DROP: route by extension, copy, import.
            // ----------------------------------------------------------------
            importSingleFile(item.path, assetsRoot, fsys);
        }
    }

    {
        std::lock_guard<std::mutex> lk(m_progressMutex);
        m_progress.current = static_cast<int>(items.size());
        m_progress.active  = false;
    }

    CoUninitialize();
    m_allDone.store(true);
}

// ---------------------------------------------------------------------------
// Update — called every frame on the main thread
// ---------------------------------------------------------------------------
void DragDropManager::Update() {
    // Detect worker completion
    if (m_allDone.exchange(false)) {
        if (app && app->getAssets())
            app->getAssets()->refreshAssets();

        if (m_refreshCallback)
            m_refreshCallback();

        {
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress.showComplete = true;
        }
        m_showingComplete = true;
        m_completeTime    = std::chrono::steady_clock::now();
    }

    // Expire the "complete" banner after kCompleteBannerSecs seconds
    if (m_showingComplete) {
        auto elapsed = std::chrono::steady_clock::now() - m_completeTime;
        float secs   = std::chrono::duration<float>(elapsed).count();
        if (secs >= kCompleteBannerSecs) {
            m_showingComplete = false;
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress = {};
        }
    }
}

// ---------------------------------------------------------------------------
// GetProgress — thread-safe snapshot
// ---------------------------------------------------------------------------
DragDropManager::ImportProgress DragDropManager::GetProgress() {
    std::lock_guard<std::mutex> lk(m_progressMutex);
    return m_progress;
}

// ---------------------------------------------------------------------------
// Shutdown — join worker before module teardown
// ---------------------------------------------------------------------------
void DragDropManager::Shutdown() {
    std::lock_guard<std::mutex> lk(m_workerMutex);
    if (m_worker.joinable()) m_worker.join();
}
