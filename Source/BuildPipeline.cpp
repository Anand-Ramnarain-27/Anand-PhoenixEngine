#include "Globals.h"
#include "BuildPipeline.h"
#include "BuildSettings.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

// Returns the process exit code, or -1 if it couldn't be launched at all.
static int execProcess(const std::string& cmdLine, std::string* capturedOutput){
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (capturedOutput){
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return -1;
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOA si{ sizeof(si) };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (capturedOutput){
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;
    }

    PROCESS_INFORMATION pi{};
    std::string mutableCmd = cmdLine;
    BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr, capturedOutput ? TRUE : FALSE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (writePipe) CloseHandle(writePipe);
    if (!ok){
        if (readPipe) CloseHandle(readPipe);
        return -1;
    }

    if (capturedOutput){
        char buf[4096];
        DWORD bytesRead = 0;
        capturedOutput->clear();
        while (ReadFile(readPipe, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0)
            capturedOutput->append(buf, bytesRead);
        CloseHandle(readPipe);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
}

BuildPipeline& BuildPipeline::Get(){
    static BuildPipeline instance;
    return instance;
}

BuildPipeline::~BuildPipeline(){
    if (m_thread.joinable()) m_thread.join();
}

std::string BuildPipeline::GetMessage() const{
    std::lock_guard<std::mutex> lock(m_msgMutex);
    return m_message;
}

void BuildPipeline::setMessage(const std::string& msg){
    std::lock_guard<std::mutex> lock(m_msgMutex);
    m_message = msg;
}

void BuildPipeline::setProgress(float progress, const std::string& msg){
    m_progress = progress;
    setMessage(msg);
}

void BuildPipeline::fail(const std::string& msg){
    setMessage(msg);
    m_status = Status::Failed;
}

void BuildPipeline::StartBuild(const BuildSettings& settings){
    if (IsRunning()) return;
    if (m_thread.joinable()) m_thread.join();
    m_status = Status::Running;
    m_progress = 0.f;
    setMessage("Starting build...");
    m_thread = std::thread([this, settings](){ run(settings); });
}

bool BuildPipeline::findMSBuild(std::string& outPath){
    char programFilesX86[MAX_PATH] = {};
    if (GetEnvironmentVariableA("ProgramFiles(x86)", programFilesX86, MAX_PATH) == 0) return false;

    std::string vswherePath = std::string(programFilesX86) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (!fs::exists(vswherePath)) return false;

    std::string cmd = "\"" + vswherePath + "\" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";
    std::string output;
    if (execProcess(cmd, &output) != 0) return false;

    size_t start = output.find_first_not_of("\r\n");
    if (start == std::string::npos) return false;
    size_t end = output.find_first_of("\r\n", start);
    outPath = output.substr(start, end == std::string::npos ? std::string::npos : end - start);
    return !outPath.empty() && fs::exists(outPath);
}

bool BuildPipeline::runMSBuild(const std::string& msbuildPath, const std::string& slnPath,
                                const std::string& target, const std::string& config, const std::string& platform){
    std::string cmd = "\"" + msbuildPath + "\" \"" + slnPath + "\" \"/t:" + target + "\" "
        "/p:Configuration=" + config + " /p:Platform=" + platform + " /m /nologo /v:quiet";
    std::string output;
    bool ok = execProcess(cmd, &output) == 0;
    if (!ok) setMessage("MSBuild failed:\n" + output.substr(0, 2000));
    return ok;
}

// robocopy handles OneDrive "Files On-Demand" cloud placeholder reparse points correctly;
// std::filesystem::copy does not (it throws filesystem_error trying to resolve them as symlinks).
static bool runRobocopy(const std::string& src, const std::string& dst, const std::string& extraArgs, std::string& outMessage){
    char sys32[MAX_PATH] = {};
    GetSystemDirectoryA(sys32, MAX_PATH);
    std::string robocopyPath = std::string(sys32) + "\\robocopy.exe";

    std::string cmd = "\"" + robocopyPath + "\" \"" + src + "\" \"" + dst + "\" " + extraArgs +
        " /R:2 /W:1 /NFL /NDL /NJH /NJS /NC /NS /NP";
    std::string output;
    int exitCode = execProcess(cmd, &output);
    // robocopy exit codes 0-7 are success (bit flags for files copied/extra/mismatched); 8+ is failure.
    if (exitCode >= 8 || exitCode < 0){
        outMessage = "Copy failed (robocopy exit " + std::to_string(exitCode) + "):\n" + output.substr(0, 2000);
        return false;
    }
    return true;
}

bool BuildPipeline::copyDirectoryContents(const std::string& src, const std::string& dst){
    if (!fs::exists(src)) return true;
    std::error_code ec;
    fs::create_directories(dst, ec);

    std::string msg;
    if (!runRobocopy(src, dst, "/MIR", msg)){ setMessage(msg); return false; }
    return true;
}

void BuildPipeline::run(BuildSettings settings){
    if (settings.getEnabledSceneCount() == 0){ fail("No enabled scenes in the build list."); return; }
    if (settings.outputDir.empty()){ fail("No output folder set."); return; }

    setProgress(0.02f, "Locating MSBuild...");
    std::string msbuildPath;
    if (!findMSBuild(msbuildPath)){ fail("Could not locate MSBuild.exe. Is Visual Studio 2022 installed?"); return; }

    char exePathBuf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    fs::path repoRoot = fs::path(exePathBuf).parent_path().parent_path().parent_path().parent_path().parent_path();
    fs::path slnPath = repoRoot / "Source" / "PhoenixEngine.sln";
    if (!fs::exists(slnPath)){ fail("Could not find PhoenixEngine.sln at " + slnPath.string()); return; }

    std::string assetsSrc = app->getFileSystem()->GetAssetsPath();
    std::string librarySrc = app->getFileSystem()->GetLibraryPath();
    fs::path outputDir(settings.outputDir);

    std::error_code ec;
    fs::path outputCanonical = fs::weakly_canonical(outputDir, ec);
    fs::path assetsCanonical = fs::weakly_canonical(assetsSrc, ec);
    fs::path repoCanonical = fs::weakly_canonical(repoRoot, ec);
    fs::path editorDirCanonical = fs::weakly_canonical(fs::path(exePathBuf).parent_path(), ec);
    const std::string outputStr = outputCanonical.string();
    if (outputCanonical == repoCanonical || outputCanonical == editorDirCanonical ||
        assetsCanonical.string().rfind(outputStr, 0) == 0){
        fail("Output folder can't be the project's own directory — pick a separate, empty folder.");
        return;
    }

    setProgress(0.08f, "Compiling Player (" + settings.configuration + "|" + settings.platform + ")... this can take a minute");
    if (!runMSBuild(msbuildPath, slnPath.string(), "Player", settings.configuration, settings.platform)){
        m_status = Status::Failed;
        return;
    }

    fs::path playerBuildDir = repoRoot / "build" / "Player" / settings.configuration / settings.platform;
    if (!fs::exists(playerBuildDir / "Player.exe")){ fail("Player.exe was not produced at " + playerBuildDir.string()); return; }

    fs::create_directories(outputDir, ec);
    if (ec){ fail("Could not create output folder: " + ec.message()); return; }

    setProgress(0.70f, "Copying Player executable and shaders...");
    {
        std::string msg;
        if (!runRobocopy(playerBuildDir.string(), outputDir.string(), "/LEV:1", msg)){ fail(msg); return; }
    }

    setProgress(0.78f, "Copying Assets...");
    if (!copyDirectoryContents(assetsSrc, (outputDir / "Assets").string())) { m_status = Status::Failed; return; }

    setProgress(0.92f, "Copying Library...");
    if (!copyDirectoryContents(librarySrc, (outputDir / "Library").string())) { m_status = Status::Failed; return; }

    setProgress(0.98f, "Writing BuildSettings.json...");
    BuildSettings shipped;
    shipped.configuration = settings.configuration;
    shipped.platform = settings.platform;
    for (const auto& e : settings.scenes)
        if (e.enabled) shipped.scenes.push_back(e);
    if (!shipped.Save((outputDir / "Library" / "BuildSettings.json").string())){
        fail("Failed writing BuildSettings.json to the output folder");
        return;
    }

    setProgress(1.f, "Build complete: " + outputDir.string());
    m_status = Status::Success;
}
