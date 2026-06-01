#pragma once
#include <filesystem>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <functional>
#include <chrono>
#include <array>

// Manages drag-and-drop file ingestion: tracks hover state, queues files for
// background import, and exposes progress state for the ImGui overlay drawn by
// ModuleEditor.
//
// Threading model:
//   - kNumWorkers persistent worker threads pull tasks from m_taskQueue.
//   - importAsset calls are serialized under m_importMutex (not guaranteed
//     thread-safe in the asset pipeline); file-copy I/O runs concurrently.
//   - Each per-file import runs in a detached sub-thread so a 30-second
//     timeout can move the queue forward even if an importer hangs.
//   - Main thread reads progress via GetProgress(), which uses try_lock and
//     falls back to a cached snapshot so it never stalls on a held mutex.
class DragDropManager {
public:
    struct ImportProgress {
        int         current      = 0;    // files completed
        int         total        = 0;
        std::string currentFile;
        bool        active       = false;
        bool        showComplete = false;
        // Last up-to-6 completed filenames for the overlay list (oldest first).
        std::vector<std::string> completedFiles;
    };

    static DragDropManager& Get();

    // Called by EngineDropTarget on DragEnter / DragLeave / DragOver.
    void SetDragging(bool dragging);
    bool IsDragging() const;

    struct DropItem {
        std::filesystem::path path;
        bool isFolder = false;
    };

    // Primary entry point called by EngineDropTarget::Drop.
    void QueueItems(std::vector<DropItem> items);

    // Convenience wrapper for callers that only have individual file paths.
    void QueueFiles(const std::vector<std::filesystem::path>& paths);

    // Called every frame from ModuleEditor::preRender().
    void Update();

    // Thread-safe snapshot via try_lock; returns cached copy if lock is busy.
    ImportProgress GetProgress();

    // Callback invoked on the main thread (via Update) once all imports finish.
    void SetRefreshCallback(std::function<void()> cb);

    // Called from ModuleEditor::cleanUp() — stops workers and joins them.
    void Shutdown();

    static bool IsSupportedExtension(const std::string& ext);

private:
    DragDropManager() = default;
    ~DragDropManager() = default;
    DragDropManager(const DragDropManager&)            = delete;
    DragDropManager& operator=(const DragDropManager&) = delete;

    void workerFunc();

    std::atomic<bool> m_isDragging{false};

    // Thread pool — workers start once and run until Shutdown().
    static constexpr int kNumWorkers       = 3;
    static constexpr int kImportTimeoutSecs = 30;
    std::array<std::thread, kNumWorkers> m_workerPool;
    std::atomic<bool> m_stopWorkers{false};
    std::atomic<bool> m_poolStarted{false};

    // Shared task queue consumed by pool workers.
    std::mutex              m_queueMutex;
    std::condition_variable m_queueCV;
    std::deque<DropItem>    m_taskQueue;

    // Serialises importAsset calls across concurrent workers.
    std::mutex m_importMutex;

    // Decremented by each worker when a task finishes; hits 0 when batch done.
    std::atomic<int> m_tasksRemaining{0};

    std::mutex     m_progressMutex;
    ImportProgress m_progress;
    ImportProgress m_cachedProgress;  // last snapshot taken under a successful lock

    std::atomic<bool> m_allDone{false};

    bool m_showingComplete = false;
    std::chrono::steady_clock::time_point m_completeTime;
    static constexpr float kCompleteBannerSecs = 3.0f;

    std::function<void()> m_refreshCallback;
};
