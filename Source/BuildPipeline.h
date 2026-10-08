#pragma once
// File > Build: packages a standalone game (Player.exe, shaders, Assets/, Library/ and a BuildSettings.json).

#include <string>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

struct BuildSettings;

/// Runs one build at a time on a worker thread. The editor polls GetStatus() / GetProgress() / GetMessage().
/// Uses the prebuilt Player next to the editor when there is one, otherwise compiles it from the engine solution.
class BuildPipeline {
public:
    enum class Status { Idle, Running, Success, Failed };

    static BuildPipeline& Get();

    /// Ignored while a build is running.
    void StartBuild(const BuildSettings& settings);
    Status GetStatus() const { return m_status.load(); }
    /// The current step while running; the result (or the failure reason) afterwards.
    std::string GetMessage() const;
    /// 0..1.
    float GetProgress() const { return m_progress.load(); }
    bool IsRunning() const { return m_status.load() == Status::Running; }

private:
    struct PlayerSource {
        bool prebuilt = false;
        std::filesystem::path prebuiltDir;   // <editor folder>/Player
        std::filesystem::path repoRoot;      // engine repo, when compiling from source
        std::filesystem::path slnPath;
    };

    BuildPipeline() = default;
    ~BuildPipeline();
    BuildPipeline(const BuildPipeline&) = delete;
    BuildPipeline& operator=(const BuildPipeline&) = delete;

    void run(BuildSettings settings);
    void setMessage(const std::string& msg);
    void setProgress(float progress, const std::string& msg);
    void fail(const std::string& msg);

    // Steps of run(); each returns false after recording the failure.
    bool locatePlayer(const std::filesystem::path& editorDir, PlayerSource& out);
    bool checkOutputDir(const std::filesystem::path& outputDir, const std::filesystem::path& editorDir,
                        const std::filesystem::path& repoRoot, const std::string& assetsSrc);
    bool preparePlayer(const BuildSettings& settings, const PlayerSource& source, std::filesystem::path& outBuildDir);
    bool copyPlayer(const std::filesystem::path& playerBuildDir, const std::filesystem::path& outputDir,
                    const std::string& productName);
    bool copyProjectData(const BuildSettings& settings, const std::string& assetsSrc, const std::string& librarySrc,
                         const std::filesystem::path& outputDir);
    bool writeShippedSettings(const BuildSettings& settings, const std::string& librarySrc,
                              const std::filesystem::path& outputDir);

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
