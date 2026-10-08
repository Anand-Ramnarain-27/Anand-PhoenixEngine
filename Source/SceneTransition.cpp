#include "Globals.h"
#include "SceneTransition.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleFileSystem.h"
#include "ModuleInput.h"
#include "ModuleUI.h"
#include "RuntimeCore.h"
#include "SceneManager.h"
#include "3rdParty/rapidjson/document.h"
#include <psapi.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#pragma comment(lib, "psapi.lib")

using Phase = SceneTransitionState::Phase;

namespace {
    constexpr const char* kConfigFile = "Settings/scene_transition.json";   // under the Assets folder
    constexpr const char* kLogFile = "scene_transitions.log";               // working directory
    constexpr double kLongLoadWarnMs = 4000.0;

    double msBetween(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b){
        return std::chrono::duration<double, std::milli>(b - a).count();
    }
    double secondsSince(std::chrono::steady_clock::time_point t){
        return msBetween(t, std::chrono::steady_clock::now()) * 0.001;
    }

    size_t workingSetBytes(){
        PROCESS_MEMORY_COUNTERS pmc = {};
        pmc.cb = sizeof(pmc);
        return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) ? pmc.WorkingSetSize : 0;
    }
    double toMB(size_t bytes){ return double(bytes) / (1024.0 * 1024.0); }

    std::string sceneName(const std::string& path){
        return path.empty() ? std::string("(unsaved)") : std::filesystem::path(path).stem().string();
    }

    // Every line goes to the log (debugger output and editor console) and to scene_transitions.log, so the
    // numbers are readable from the standalone player too.
    void report(const char* format, ...){
        char line[1024];
        va_list ap;
        va_start(ap, format);
        vsnprintf(line, sizeof(line), format, ap);
        va_end(ap);

        PHX_LOG(Scene, Info, "%s", line);
        if (FILE* f = nullptr; fopen_s(&f, kLogFile, "a") == 0 && f){
            fprintf(f, "%s\n", line);
            fclose(f);
        }
    }

    void readFloat(const rapidjson::Value& doc, const char* key, float& out){
        if (doc.HasMember(key) && doc[key].IsNumber()) out = std::max(0.f, doc[key].GetFloat());
    }
}

void SceneTransition::loadConfig(){
    m_cfg = Config{};
    const std::string path = app->getFileSystem()->GetAssetsPath() + kConfigFile;
    std::ifstream in(path, std::ios::binary);
    if (!in) return;   // defaults
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(text.c_str());
    if (doc.HasParseError() || !doc.IsObject()){
        report("[SceneTransition] WARNING: %s is not valid JSON - using default timings", path.c_str());
        return;
    }
    readFloat(doc, "fadeOutSeconds", m_cfg.fadeOutSeconds);
    readFloat(doc, "fadeInSeconds", m_cfg.fadeInSeconds);
    readFloat(doc, "minBlackSeconds", m_cfg.minBlackSeconds);
    readFloat(doc, "warmUpTimeoutSeconds", m_cfg.warmUpTimeoutSeconds);
    if (doc.HasMember("warmUpFrames") && doc["warmUpFrames"].IsInt())
        m_cfg.warmUpFrames = std::max(0, doc["warmUpFrames"].GetInt());
}

bool SceneTransition::update(SceneManager& sm, RuntimeCore& rc){
    SceneTransitionState& t = sm.getTransition();
    switch (t.phase){
    case Phase::Idle:
        if (!t.requestedPath.empty()) begin(sm);
        return false;

    case Phase::FadingOut: {
        const double a = m_cfg.fadeOutSeconds > 0.f ? secondsSince(m_phaseStart) / m_cfg.fadeOutSeconds : 1.0;
        if (a >= 1.0){
            // This frame renders and presents at full black; the load starts on the next tick.
            t.fadeAlpha = 1.f;
            t.phase = Phase::HoldBlack;
            m_blackStart = Clock::now();
        }
        else t.fadeAlpha = float(a);
        return false;
    }

    case Phase::HoldBlack:
        // A whole frame at alpha 1 has been presented since FadingOut ended.
        t.phase = Phase::Loading;
        runLoad(sm, rc);
        return true;

    case Phase::Loading:
        return false;   // runLoad() leaves Loading before returning

    case Phase::WarmUp: {
        const Clock::time_point now = Clock::now();
        if (m_warmFrames == 0) m_firstWarmFrameMs = msBetween(m_lastWarmTick, now);
        // Scripts only run while playing: a paused editor holds the black screen instead of timing out.
        if (sm.getState() != SceneManager::PlayState::Playing){ m_phaseStart = now; return false; }
        ++m_warmFrames;
        if (t.readySignalled) ++m_readyFrames;
        // m_readyFrames - 1 frames have rendered since the level reported ready.
        const bool ready = t.readySignalled && m_readyFrames > m_cfg.warmUpFrames;
        const bool timedOut = !ready && m_warmFrames > m_cfg.warmUpFrames &&
                              secondsSince(m_phaseStart) >= m_cfg.warmUpTimeoutSeconds;
        if ((ready || timedOut) && secondsSince(m_blackStart) >= m_cfg.minBlackSeconds) beginFadeIn(sm, timedOut);
        return false;
    }

    case Phase::FadingIn: {
        const double a = m_cfg.fadeInSeconds > 0.f ? 1.0 - secondsSince(m_phaseStart) / m_cfg.fadeInSeconds : 0.0;
        if (a <= 0.0) finish(sm);
        else t.fadeAlpha = float(a);
        return false;
    }
    }
    return false;
}

void SceneTransition::begin(SceneManager& sm){
    SceneTransitionState& t = sm.getTransition();
    loadConfig();
    m_to = t.requestedPath;
    t.requestedPath.clear();
    m_from = sm.getCurrentScenePath();
    m_requestTime = m_phaseStart = Clock::now();
    m_warmFrames = m_readyFrames = 0;
    m_loadMs = m_firstWarmFrameMs = 0.0;
    m_workingSetBefore = workingSetBytes();
    t.readySignalled = false;
    t.fadeAlpha = 0.f;
    t.phase = Phase::FadingOut;
    report("[SceneTransition] %s -> %s: fading out (%.2f s)", sceneName(m_from).c_str(), sceneName(m_to).c_str(),
           m_cfg.fadeOutSeconds);
}

void SceneTransition::runLoad(SceneManager& sm, RuntimeCore& rc){
    SceneTransitionState& t = sm.getTransition();
    const Clock::time_point t0 = Clock::now();

    // Unload safety: wait for every submitted frame before the old scene's GPU resources are freed (loadScene
    // flushes too; doing it here first keeps the GPU wait out of the scene-load number).
    app->getD3D12()->flush();
    const Clock::time_point t1 = Clock::now();

    const bool loaded = sm.replaceScene(m_to);
    const Clock::time_point t2 = Clock::now();

    if (loaded) rc.applySkyboxFromSettings();
    const Clock::time_point t3 = Clock::now();

    m_loadMs = msBetween(t0, t3);
    m_longestLoadMs = std::max(m_longestLoadMs, m_loadMs);
    report("[SceneTransition] %s -> %s: blocking load %.0f ms (GPU flush %.0f, scene read+unload+build %.0f, skybox %.0f)%s",
           sceneName(m_from).c_str(), sceneName(m_to).c_str(), m_loadMs, msBetween(t0, t1), msBetween(t1, t2),
           msBetween(t2, t3), loaded ? "" : " - LOAD FAILED, fading back in");
    if (m_loadMs > kLongLoadWarnMs)
        report("[SceneTransition] WARNING: blocking load took %.1f s (over %.0f s)", m_loadMs * 0.001, kLongLoadWarnMs * 0.001);

    if (!loaded) t.readySignalled = true;   // nothing new will report ready
    t.phase = Phase::WarmUp;
    m_phaseStart = m_lastWarmTick = Clock::now();
}

void SceneTransition::beginFadeIn(SceneManager& sm, bool timedOut){
    SceneTransitionState& t = sm.getTransition();
    const double warmUpMs = msBetween(m_lastWarmTick, Clock::now());
    const size_t ws = workingSetBytes();
    report("[SceneTransition] %s: warm-up %d frames / %.0f ms (first frame %.0f ms)%s, black %.0f ms, working set %.1f MB (%+.1f MB)",
           sceneName(m_to).c_str(), m_warmFrames, warmUpMs, m_firstWarmFrameMs,
           timedOut ? " - TIMED OUT waiting for NotifyTransitionReady" : "",
           secondsSince(m_blackStart) * 1000.0, toMB(ws), toMB(ws) - toMB(m_workingSetBefore));

    // Anything pressed during the load stays held, never a fresh press, once input is released.
    app->getInput()->resetState();
    if (ModuleUI* ui = app->getUI()){ ui->takeEditKeys(); ui->takeTypedText(); }

    t.phase = Phase::FadingIn;
    t.fadeAlpha = 1.f;
    m_phaseStart = Clock::now();
}

void SceneTransition::finish(SceneManager& sm){
    SceneTransitionState& t = sm.getTransition();
    t.phase = Phase::Idle;
    t.fadeAlpha = 0.f;
    t.readySignalled = false;
    report("[SceneTransition] %s -> %s: done in %.0f ms (longest blocking load this session: %.0f ms)",
           sceneName(m_from).c_str(), sceneName(m_to).c_str(), secondsSince(m_requestTime) * 1000.0, m_longestLoadMs);
}
