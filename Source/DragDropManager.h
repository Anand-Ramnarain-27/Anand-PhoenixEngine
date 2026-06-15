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

class DragDropManager {
public:
    struct ImportProgress {
        int current = 0;
        int total = 0;
        std::string currentFile;
        bool active = false;
        bool showComplete = false;
        std::vector<std::string> completedFiles;
    };

    static DragDropManager& Get();

    void SetDragging(bool dragging);
    bool IsDragging() const;

    struct DropItem {
        std::filesystem::path path;
        bool isFolder = false;
    };

    void QueueItems(std::vector<DropItem> items);

    void QueueFiles(const std::vector<std::filesystem::path>& paths);

    void Update();

    ImportProgress GetProgress();

    void SetRefreshCallback(std::function<void()> cb);

    void Shutdown();

    static bool IsSupportedExtension(const std::string& ext);

private:
    DragDropManager() = default;
    ~DragDropManager() = default;
    DragDropManager(const DragDropManager&) = delete;
    DragDropManager& operator=(const DragDropManager&) = delete;

    void workerFunc();

    std::atomic<bool> m_isDragging{false};

    static constexpr int kNumWorkers = 3;
    static constexpr int kImportTimeoutSecs = 30;
    std::array<std::thread, kNumWorkers> m_workerPool;
    std::atomic<bool> m_stopWorkers{false};
    std::atomic<bool> m_poolStarted{false};

    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    std::deque<DropItem> m_taskQueue;

    std::mutex m_importMutex;

    std::atomic<int> m_tasksRemaining{0};

    std::mutex m_progressMutex;
    ImportProgress m_progress;
    ImportProgress m_cachedProgress;

    std::atomic<bool> m_allDone{false};

    bool m_showingComplete = false;
    std::chrono::steady_clock::time_point m_completeTime;
    static constexpr float kCompleteBannerSecs = 3.0f;

    std::function<void()> m_refreshCallback;
};
