#pragma once
// Watches a directory tree for file changes on a background thread.

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <windows.h>

/// ReadDirectoryChangesW on a worker thread; poll() delivers the queued events to the callback on the calling
/// thread.
class FileWatcher {
public:
    enum class Event { Added, Modified, Deleted };
    using Callback = std::function<void(const std::string& absPath, Event)>;

    FileWatcher() = default;
    ~FileWatcher(){
        stop();
    }

    void start(const std::string& rootDir, Callback cb);
    void stop();
    void poll();

private:
    void watchThread();

    std::string m_root;
    Callback m_callback;

    std::thread m_thread;
    std::atomic<bool> m_running{ false };

    struct PendingEvent {
        std::string path;
        Event ev;
    };
    std::vector<PendingEvent> m_queue;
    std::vector<PendingEvent> m_swap;
    std::mutex m_mutex;

    HANDLE m_dirHandle = INVALID_HANDLE_VALUE;
    HANDLE m_stopEvent = INVALID_HANDLE_VALUE;
};
