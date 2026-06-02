#include "Globals.h"
#include "DragDropManager.h"
#include "Application.h"
#include "ModuleAssets.h"
#include "ModuleFileSystem.h"

#include <ole2.h>
#include <filesystem>
#include <algorithm>
#include <future>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Extension tables
// ---------------------------------------------------------------------------
static const char* kModelExts[] = { ".gltf", ".glb", ".fbx", ".obj", ".stl", ".blend", nullptr };
static const char* kTextureExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".dds", ".bmp", ".hdr", nullptr };
static const char* kAnimExts[] = { ".anim", nullptr };
static const char* kMatExts[] = { ".mat", nullptr };

bool DragDropManager::IsSupportedExtension(const std::string& ext) {
    for (auto** p = kModelExts; *p; ++p) if (ext == *p) return true;
    for (auto** p = kTextureExts; *p; ++p) if (ext == *p) return true;
    for (auto** p = kAnimExts; *p; ++p) if (ext == *p) return true;
    for (auto** p = kMatExts; *p; ++p) if (ext == *p) return true;
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
// Dragging state (atomic — safe from any thread)
// ---------------------------------------------------------------------------
void DragDropManager::SetDragging(bool dragging) { m_isDragging.store(dragging); }
bool DragDropManager::IsDragging() const { return m_isDragging.load(); }

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void DragDropManager::SetRefreshCallback(std::function<void()> cb) {
    m_refreshCallback = std::move(cb);
}

// ---------------------------------------------------------------------------
// QueueItems — main thread entry point
// ---------------------------------------------------------------------------
void DragDropManager::QueueItems(std::vector<DropItem> items) {
    if (items.empty()) return;

    // Filter individual files to supported extensions only.
    std::vector<DropItem> filtered;
    filtered.reserve(items.size());
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

    // Reset progress state for the new batch.
    m_allDone.store(false);
    m_showingComplete = false;
    {
        std::lock_guard<std::mutex> lk(m_progressMutex);
        m_progress = {};
        m_progress.total = static_cast<int>(filtered.size());
        m_progress.active = true;
    }
    m_tasksRemaining.store(static_cast<int>(filtered.size()));

    // Push all tasks into the shared queue.
    {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        for (auto& item : filtered)
            m_taskQueue.push_back(std::move(item));
    }

    // Start the thread pool on the first call; thereafter just wake workers.
    if (!m_poolStarted.exchange(true)) {
        m_stopWorkers.store(false);
        for (int i = 0; i < kNumWorkers; ++i)
            m_workerPool[i] = std::thread(&DragDropManager::workerFunc, this);
    }
    m_queueCV.notify_all();
}

// Convenience wrapper for callers that only deal with individual file paths.
void DragDropManager::QueueFiles(const std::vector<fs::path>& paths) {
    std::vector<DropItem> items;
    items.reserve(paths.size());
    for (const auto& p : paths) items.push_back({ p, false });
    QueueItems(std::move(items));
}

// ---------------------------------------------------------------------------
// Worker thread — persistent, pulls one task at a time from the shared queue.
// ---------------------------------------------------------------------------

// Handles a single file copy + importAsset call.  Called from a detached
// sub-thread so that the 30-second timeout can abandon a hanging importer
// without blocking the pool worker.
static void importFileTask(const fs::path& srcPath,
                            const std::string& assetsRoot,
                            ModuleFileSystem* fsys,
                            std::mutex& importMutex){
    std::string ext = srcPath.extension().string();
    for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    std::string subDir;
    for (auto** p = kModelExts; *p; ++p) if (ext == *p) { subDir = "Models/"; break; }
    for (auto** p = kTextureExts; *p; ++p) if (ext == *p) { subDir = "Textures/"; break; }
    for (auto** p = kAnimExts; *p; ++p) if (ext == *p) { subDir = "Animations/"; break; }
    for (auto** p = kMatExts; *p; ++p) if (ext == *p) { subDir = "Materials/"; break; }

    std::string destDir = assetsRoot + subDir;
    std::string destPath = destDir + srcPath.filename().string();

    fsys->CreateDir(destDir.c_str());

    bool alreadyInAssets = (srcPath.string().rfind(assetsRoot, 0) == 0);
    if (!alreadyInAssets) {
        if (!fsys->Copy(srcPath.string().c_str(), destPath.c_str())) {
            LOG("DragDrop: Failed to copy '%s' -> '%s'",
                srcPath.string().c_str(), destPath.c_str());
            return;
        }
        LOG("DragDrop: Copied '%s' -> '%s'", srcPath.string().c_str(), destPath.c_str());
    } else {
        destPath = srcPath.string();
    }

    // importAsset is serialized across all workers via importMutex.
    std::lock_guard<std::mutex> lk(importMutex);
    UID uid = app->getAssets()->importAsset(destPath.c_str());
    if (uid != 0)
    {
        LOG("DragDrop: Imported '%s' (uid=%llu)", destPath.c_str(), uid);
    }
    else
    {
        LOG("DragDrop: importAsset returned 0 for '%s'", destPath.c_str());
    }
}

static void importFolderTask(const fs::path& srcFolder,
                              const std::string& assetsRoot,
                              ModuleFileSystem* fsys,
                              std::mutex& importMutex){
    std::string folderName = srcFolder.filename().string();
    fs::path destRoot = fs::path(assetsRoot + "Models/") / folderName;

    bool alreadyInAssets = (srcFolder.string().rfind(assetsRoot, 0) == 0);
    if (!alreadyInAssets) {
        try {
            fs::copy(srcFolder, destRoot,
                     fs::copy_options::recursive |
                     fs::copy_options::overwrite_existing);
            LOG("DragDrop: Copied folder '%s' -> '%s'",
                srcFolder.string().c_str(), destRoot.string().c_str());
        } catch (const std::exception& ex) {
            LOG("DragDrop: Failed to copy folder '%s': %s",
                srcFolder.string().c_str(), ex.what());
            return;
        }
    } else {
        destRoot = srcFolder;
    }

    // Import model files found anywhere in the copied folder (serialized).
    try {
        for (const auto& entry : fs::recursive_directory_iterator(destRoot)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            bool isModel = false;
            for (auto** p = kModelExts; *p; ++p) if (ext == *p) { isModel = true; break; }
            if (!isModel) continue;

            std::lock_guard<std::mutex> lk(importMutex);
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
        LOG("DragDrop: Error walking copied folder '%s'", destRoot.string().c_str());
    }
}

void DragDropManager::workerFunc() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    while (true) {
        // Pull next task (blocks until work is available or stop is signalled).
        DropItem item;
        {
            std::unique_lock<std::mutex> lk(m_queueMutex);
            m_queueCV.wait(lk, [this] {
                return !m_taskQueue.empty() || m_stopWorkers.load();
            });
            if (m_stopWorkers.load() && m_taskQueue.empty()) break;
            if (m_taskQueue.empty()) continue;
            item = std::move(m_taskQueue.front());
            m_taskQueue.pop_front();
        }

        std::string fname = item.path.filename().string();

        // Update the "currently importing" display name.
        {
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress.currentFile = fname;
        }

        if (app && app->getAssets() && app->getFileSystem()) {
            const std::string assetsRoot = app->getFileSystem()->GetAssetsPath();
            ModuleFileSystem* fsys = app->getFileSystem();

            // Run the import inside a detached sub-thread so we can enforce
            // the per-file timeout without blocking the pool worker forever.
            std::promise<void> donePromise;
            auto doneFuture = donePromise.get_future();
            const bool isFolder = item.isFolder;

            std::thread importThread(
                [this, item = std::move(item), assetsRoot, fsys, isFolder,
                 p = std::move(donePromise)]() mutable
            {
                CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (isFolder)
                    importFolderTask(item.path, assetsRoot, fsys, m_importMutex);
                else
                    importFileTask(item.path, assetsRoot, fsys, m_importMutex);
                CoUninitialize();
                try { p.set_value(); } catch (...) {}
            });
            importThread.detach();

            if (doneFuture.wait_for(std::chrono::seconds(kImportTimeoutSecs)) ==
                std::future_status::timeout){
                LOG("DragDrop: Import of '%s' exceeded %ds, skipping to next file.",
                    fname.c_str(), kImportTimeoutSecs);
            }
        }

        // Record completion and check whether the whole batch is finished.
        {
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress.current++;
            m_progress.completedFiles.push_back(fname);
            if (m_progress.completedFiles.size() > 6)
                m_progress.completedFiles.erase(m_progress.completedFiles.begin());
        }

        if (--m_tasksRemaining == 0) {
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress.active = false;
            m_allDone.store(true);
        }
    }

    CoUninitialize();
}

// ---------------------------------------------------------------------------
// Update — called every frame on the main thread
// ---------------------------------------------------------------------------
void DragDropManager::Update() {
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
        m_completeTime = std::chrono::steady_clock::now();
    }

    if (m_showingComplete) {
        float secs = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_completeTime).count();
        if (secs >= kCompleteBannerSecs) {
            m_showingComplete = false;
            std::lock_guard<std::mutex> lk(m_progressMutex);
            m_progress = {};
        }
    }
}

// ---------------------------------------------------------------------------
// GetProgress — try_lock; falls back to cached copy so the main thread never
// stalls waiting for a worker that holds the progress mutex.
// ---------------------------------------------------------------------------
DragDropManager::ImportProgress DragDropManager::GetProgress() {
    if (m_progressMutex.try_lock()) {
        m_cachedProgress = m_progress;
        m_progressMutex.unlock();
    }
    return m_cachedProgress;
}

// ---------------------------------------------------------------------------
// Shutdown — drain the queue and join all pool workers
// ---------------------------------------------------------------------------
void DragDropManager::Shutdown() {
    {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        m_taskQueue.clear();
        m_stopWorkers.store(true);
    }
    m_queueCV.notify_all();

    for (auto& t : m_workerPool)
        if (t.joinable()) t.join();
}
