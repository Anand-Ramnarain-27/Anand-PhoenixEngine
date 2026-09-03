#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

struct BuildSettings;

class BuildPipeline {
public:
    enum class Status { Idle, Running, Success, Failed };

    static BuildPipeline& Get();

    void StartBuild(const BuildSettings& settings);
    Status GetStatus() const { return m_status.load(); }
    std::string GetMessage() const;
    float GetProgress() const { return m_progress.load(); }
    bool IsRunning() const { return m_status.load() == Status::Running; }

private:
    BuildPipeline() = default;
    ~BuildPipeline();
    BuildPipeline(const BuildPipeline&) = delete;
    BuildPipeline& operator=(const BuildPipeline&) = delete;

    void run(BuildSettings settings);
    void setMessage(const std::string& msg);
    void setProgress(float progress, const std::string& msg);
    void fail(const std::string& msg);

    bool findMSBuild(std::string& outPath);
    bool runMSBuild(const std::string& msbuildPath, const std::string& slnPath,
                     const std::string& target, const std::string& config, const std::string& platform);
    bool copyDirectoryContents(const std::string& src, const std::string& dst);

    std::thread m_thread;
    std::atomic<Status> m_status{ Status::Idle };
    std::atomic<float> m_progress{ 0.f };
    mutable std::mutex m_msgMutex;
    std::string m_message;
};
