#pragma once
#include <filesystem>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <functional>
#include <chrono>

// Manages drag-and-drop file ingestion: tracks hover state, queues files for
// background import, and exposes progress state for the ImGui overlay drawn by
// ModuleEditor.  Thread-safety contract: worker writes progress fields under
// m_progressMutex; main thread reads via GetProgress().  All other methods are
// main-thread-only unless noted.
class DragDropManager {
public:
    struct ImportProgress {
        int         current      = 0;
        int         total        = 0;
        std::string currentFile;
        bool        active       = false;  // worker is running
        bool        showComplete = false;  // done, panel shown briefly
    };

    static DragDropManager& Get();

    // Called by EngineDropTarget on DragEnter / DragLeave / DragOver.
    void SetDragging(bool dragging);
    bool IsDragging() const;

    // A single item from a drop event — either an individual file or a whole
    // folder.  Folders are copied intact to Assets/Models/<name>/ to preserve
    // relative texture / .bin references; individual files are routed by
    // extension.
    struct DropItem {
        std::filesystem::path path;
        bool isFolder = false;
    };

    // Primary entry point called by EngineDropTarget::Drop.
    // Folders must NOT be pre-expanded; pass them as DropItem{path, true} so
    // the worker can copy the entire directory tree.
    void QueueItems(std::vector<DropItem> items);

    // Convenience wrapper for callers that only have individual file paths.
    void QueueFiles(const std::vector<std::filesystem::path>& paths);

    // Called every frame from ModuleEditor::preRender(). Handles completion
    // detection and triggers the asset-browser refresh callback.
    void Update();

    // Thread-safe snapshot of current import state (safe to call any frame).
    ImportProgress GetProgress();

    // Callback invoked on main thread (via Update) once all imports finish.
    void SetRefreshCallback(std::function<void()> cb);

    // Called from ModuleEditor::cleanUp() — blocks until the worker exits.
    void Shutdown();

    // Returns true for extensions the importer pipeline can handle.
    static bool IsSupportedExtension(const std::string& ext);

private:
    DragDropManager() = default;
    ~DragDropManager() = default;
    DragDropManager(const DragDropManager&)            = delete;
    DragDropManager& operator=(const DragDropManager&) = delete;

    void workerProc(std::vector<DropItem> items);

    std::atomic<bool> m_isDragging{false};

    std::mutex  m_workerMutex;   // serialises start / join of m_worker
    std::thread m_worker;

    std::mutex     m_progressMutex;
    ImportProgress m_progress;   // written by worker, read by main via GetProgress()

    std::atomic<bool> m_allDone{false};  // worker sets true when finished

    // main-thread state for the "complete" banner timer
    bool m_showingComplete  = false;
    std::chrono::steady_clock::time_point m_completeTime;
    static constexpr float kCompleteBannerSecs = 3.0f;

    std::function<void()> m_refreshCallback;
};
