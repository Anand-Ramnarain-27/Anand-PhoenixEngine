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

// Extensions already re-compiled into Library/ (Meshes/Materials/Animations/Textures) and safe
// to skip when shipping raw Assets. Scenes (.json), scripts (.dll/.pdb), skybox HDRs (.hdr), and
// state machines (.json) are deliberately NOT here — those are loaded directly by path at runtime.
static const char* kStrippableAssetExtensions =
    "*.fbx *.stl *.blend *.gltf *.glb *.png *.jpg *.jpeg *.tga *.bmp *.dds";

static std::string sanitizeFileStem(const std::string& name){
    std::string out;
    for (char c : name){
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
            continue;
        out += c;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    return out.empty() ? "Player" : out;
}

// Where the Player comes from:
//  1. Prebuilt: <editor folder>/Player/Player.exe (+ its .cso shaders). A game project that ships the editor as a
//     copied exe (ashfall/engine) gets it from tools/release/Export-Release.ps1 - no engine source needed.
//  2. Compiled: the engine's Source/PhoenixEngine.sln, found by walking up from the editor (the engine repo's own
//     build/PhoenixEngine/<cfg>/x64 layout), also looking beside each folder on the way (a sibling engine repo).
bool BuildPipeline::locatePlayer(const fs::path& editorDir, PlayerSource& out){
    out.prebuiltDir = editorDir / "Player";
    out.prebuilt = fs::exists(out.prebuiltDir / "Player.exe");
    if (out.prebuilt) return true;

    std::error_code findEc;
    for (fs::path dir = editorDir; !dir.empty() && out.slnPath.empty(); dir = dir.parent_path()){
        if (fs::exists(dir / "Source" / "PhoenixEngine.sln")){ out.repoRoot = dir; out.slnPath = dir / "Source" / "PhoenixEngine.sln"; break; }
        for (const auto& sib : fs::directory_iterator(dir, findEc)){
            if (sib.is_directory(findEc) && fs::exists(sib.path() / "Source" / "PhoenixEngine.sln")){
                out.repoRoot = sib.path();
                out.slnPath = out.repoRoot / "Source" / "PhoenixEngine.sln";
                break;
            }
        }
        if (dir == dir.parent_path()) break;
    }
    if (out.slnPath.empty()){
        fail("No prebuilt Player (" + (out.prebuiltDir / "Player.exe").string() + ") and no engine source "
             "(Source/PhoenixEngine.sln) found from " + editorDir.string() + ". Run tools/release/Export-Release.ps1 "
             "in the engine repo to put a prebuilt Player next to this editor.");
        return false;
    }
    return true;
}

bool BuildPipeline::checkOutputDir(const fs::path& outputDir, const fs::path& editorDir, const fs::path& repoRoot,
                                   const std::string& assetsSrc){
    std::error_code ec;
    fs::path outputCanonical = fs::weakly_canonical(outputDir, ec);
    fs::path assetsCanonical = fs::weakly_canonical(assetsSrc, ec);
    fs::path repoCanonical = repoRoot.empty() ? fs::path() : fs::weakly_canonical(repoRoot, ec);
    fs::path editorDirCanonical = fs::weakly_canonical(editorDir, ec);
    const std::string outputStr = outputCanonical.string();
    if ((!repoRoot.empty() && outputCanonical == repoCanonical) || outputCanonical == editorDirCanonical ||
        assetsCanonical.string().rfind(outputStr, 0) == 0){
        fail("Output folder can't be the project's own directory — pick a separate, empty folder.");
        return false;
    }
    return true;
}

bool BuildPipeline::preparePlayer(const BuildSettings& settings, const PlayerSource& source, fs::path& outBuildDir){
    if (source.prebuilt){
        // Exported as a Release build; the Configuration setting only matters when compiling from source.
        setProgress(0.08f, "Using the prebuilt Player (" + source.prebuiltDir.string() + ")");
        outBuildDir = source.prebuiltDir;
        return true;
    }
    setProgress(0.04f, "Locating MSBuild...");
    std::string msbuildPath;
    if (!findMSBuild(msbuildPath)){ fail("Could not locate MSBuild.exe. Is Visual Studio 2022 installed?"); return false; }
    setProgress(0.08f, "Compiling Player (" + settings.configuration + "|" + settings.platform + ")... this can take a minute");
    if (!runMSBuild(msbuildPath, source.slnPath.string(), "Player", settings.configuration, settings.platform)){
        m_status = Status::Failed;
        return false;
    }
    outBuildDir = source.repoRoot / "build" / "Player" / settings.configuration / settings.platform;
    if (!fs::exists(outBuildDir / "Player.exe")){ fail("Player.exe was not produced at " + outBuildDir.string()); return false; }
    return true;
}

bool BuildPipeline::copyPlayer(const fs::path& playerBuildDir, const fs::path& outputDir, const std::string& productName){
    setProgress(0.70f, "Copying Player executable and shaders...");
    std::string msg;
    if (!runRobocopy(playerBuildDir.string(), outputDir.string(), "/LEV:1", msg)){ fail(msg); return false; }

    const std::string stem = sanitizeFileStem(productName);
    if (stem != "Player"){
        std::error_code renEc;
        if (fs::exists(outputDir / "Player.exe"))
            fs::rename(outputDir / "Player.exe", outputDir / (stem + ".exe"), renEc);
        if (fs::exists(outputDir / "Player.pdb"))
            fs::rename(outputDir / "Player.pdb", outputDir / (stem + ".pdb"), renEc);
    }
    return true;
}

bool BuildPipeline::copyProjectData(const BuildSettings& settings, const std::string& assetsSrc,
                                    const std::string& librarySrc, const fs::path& outputDir){
    setProgress(0.78f, "Copying Assets...");
    if (settings.stripSourceAssets){
        std::error_code assetsEc;
        fs::create_directories(outputDir / "Assets", assetsEc);
        std::string msg;
        std::string args = std::string("/MIR /XF ") + kStrippableAssetExtensions;
        if (!runRobocopy(assetsSrc, (outputDir / "Assets").string(), args, msg)){ fail(msg); return false; }
    }
    else if (!copyDirectoryContents(assetsSrc, (outputDir / "Assets").string())){ m_status = Status::Failed; return false; }

    setProgress(0.92f, "Copying Library...");
    if (!copyDirectoryContents(librarySrc, (outputDir / "Library").string())){ m_status = Status::Failed; return false; }
    return true;
}

bool BuildPipeline::writeShippedSettings(const BuildSettings& settings, const std::string& librarySrc, const fs::path& outputDir){
    setProgress(0.98f, "Writing BuildSettings.json...");
    BuildSettings shipped;
    shipped.configuration = settings.configuration;
    shipped.platform = settings.platform;
    for (auto e : settings.scenes){
        if (!e.enabled) continue;
        // Shipped paths are relative to the output folder; the Player resolves them the same way.
        fs::path rel(e.path);
        if (rel.is_absolute()){
            std::error_code relEc;
            fs::path r = fs::relative(rel, fs::path(librarySrc).parent_path().parent_path(), relEc);
            if (!relEc && !r.empty()) rel = r;
        }
        if (!fs::exists(outputDir / rel)){
            const fs::path inLibrary = fs::path("Library") / "Scenes" / rel.filename();
            if (!fs::exists(outputDir / inLibrary)){
                fail("Scene in the build list not found: " + e.path + " (scenes live in Library/Scenes - re-add it)");
                return false;
            }
            rel = inLibrary;
        }
        e.path = rel.generic_string();
        shipped.scenes.push_back(e);
    }
    if (!shipped.Save((outputDir / "Library" / "BuildSettings.json").string())){
        fail("Failed writing BuildSettings.json to the output folder");
        return false;
    }
    return true;
}

void BuildPipeline::run(BuildSettings settings){
    if (settings.getEnabledSceneCount() == 0){ fail("No enabled scenes in the build list."); return; }
    if (settings.outputDir.empty()){ fail("No output folder set."); return; }

    char exePathBuf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    const fs::path editorDir = fs::path(exePathBuf).parent_path();

    PlayerSource source;
    if (!locatePlayer(editorDir, source)) return;

    const std::string assetsSrc = app->getFileSystem()->GetAssetsPath();
    const std::string librarySrc = app->getFileSystem()->GetLibraryPath();
    const fs::path outputDir(settings.outputDir);
    if (!checkOutputDir(outputDir, editorDir, source.repoRoot, assetsSrc)) return;

    fs::path playerBuildDir;
    if (!preparePlayer(settings, source, playerBuildDir)) return;

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec){ fail("Could not create output folder: " + ec.message()); return; }

    if (!copyPlayer(playerBuildDir, outputDir, settings.productName)) return;
    if (!copyProjectData(settings, assetsSrc, librarySrc, outputDir)) return;
    if (!writeShippedSettings(settings, librarySrc, outputDir)) return;

    setProgress(1.f, "Build complete: " + outputDir.string());
    m_status = Status::Success;
}
