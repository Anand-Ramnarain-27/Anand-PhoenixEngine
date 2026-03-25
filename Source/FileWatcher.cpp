#include "Globals.h"
#include "FileWatcher.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

void FileWatcher::start(const std::string& rootDir, Callback cb) {
    stop();

    m_root = rootDir;
    m_callback = std::move(cb);

    m_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (m_stopEvent == INVALID_HANDLE_VALUE) {
        std::cerr << "[FileWatcher] Failed to create stop event\n";
        return;
    }

    m_running = true;
    m_thread = std::thread(&FileWatcher::watchThread, this);
}

void FileWatcher::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_stopEvent != INVALID_HANDLE_VALUE) {
        SetEvent(m_stopEvent);
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_stopEvent != INVALID_HANDLE_VALUE) {
        CloseHandle(m_stopEvent);
        m_stopEvent = INVALID_HANDLE_VALUE;
    }
}

void FileWatcher::poll() {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_swap.swap(m_queue);
    }
    for (auto& ev : m_swap) {
        m_callback(ev.path, ev.ev);
    }
    m_swap.clear();
}

void FileWatcher::watchThread() {
    m_dirHandle = CreateFileA(m_root.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (m_dirHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "[FileWatcher] Cannot open directory: " << m_root << "\n";
        m_running = false;
        return;
    }

    constexpr DWORD kBufSize = 65536;
    alignas(DWORD) char buf[kBufSize];

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);

    HANDLE waitHandles[2] = { ov.hEvent, m_stopEvent };

    auto queueRead = [&]() -> bool {
        ResetEvent(ov.hEvent);
        BOOL ok = ReadDirectoryChangesW(m_dirHandle, buf, kBufSize, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &ov, nullptr);
        return ok == TRUE;
        };

    if (!queueRead()) {
        std::cerr << "[FileWatcher] ReadDirectoryChangesW failed\n";
        CloseHandle(ov.hEvent);
        CloseHandle(m_dirHandle);
        m_running = false;
        return;
    }

    while (m_running) {
        DWORD wait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (wait == WAIT_OBJECT_0 + 1 || !m_running) break;

        if (wait != WAIT_OBJECT_0) break;

        DWORD transferred = 0;
        if (!GetOverlappedResult(m_dirHandle, &ov, &transferred, FALSE) || transferred == 0) {
            if (m_running) queueRead();
            continue;
        }

        const char* cursor = buf;
        while (true) {
            const auto* fni = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);
            int needed = WideCharToMultiByte(CP_UTF8, 0, fni->FileName, (int)(fni->FileNameLength / sizeof(WCHAR)), nullptr, 0, nullptr, nullptr);
            std::string relName(needed, '\0');
            WideCharToMultiByte(CP_UTF8, 0, fni->FileName, (int)(fni->FileNameLength / sizeof(WCHAR)), relName.data(), needed, nullptr, nullptr);

            for (char& c : relName) if (c == '\\') c = '/';

            std::string absPath = m_root + relName;

            Event ev;
            bool relevant = true;
            switch (fni->Action) {
            case FILE_ACTION_ADDED:
            case FILE_ACTION_RENAMED_NEW_NAME: ev = Event::Added; break;
            case FILE_ACTION_MODIFIED: ev = Event::Modified; break;
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME: ev = Event::Deleted; break;
            default: relevant = false; break;
            }

            if (relevant) {
                bool isDir = fs::is_directory(fs::path(absPath));
                bool isMeta = absPath.size() > 5 && absPath.compare(absPath.size() - 5, 5, ".meta") == 0;

                if (!isDir && !isMeta) {
                    std::lock_guard<std::mutex> lk(m_mutex);
                    bool dup = false;
                    for (auto it = m_queue.rbegin(); it != m_queue.rend(); ++it) {
                        if (it->path == absPath) {
                            if (ev == Event::Deleted) it->ev = Event::Deleted;
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) m_queue.push_back({ absPath, ev });
                }
            }

            if (fni->NextEntryOffset == 0) break;
            cursor += fni->NextEntryOffset;
        }

        queueRead();
    }

    CloseHandle(ov.hEvent);
    CloseHandle(m_dirHandle);
    m_dirHandle = INVALID_HANDLE_VALUE;
}