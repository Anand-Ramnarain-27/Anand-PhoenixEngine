#pragma once
#include <chrono>
#include <cstddef>
#include <string>

class RuntimeCore;
class SceneManager;

// Runs the faded level change asked for through Phoenix::Scene::RequestLevelTransition. The state it drives is
// SceneManager::getTransition(), which scripts read through Phoenix::Scene:
//
//   Idle -> FadingOut -> HoldBlack -> Loading -> WarmUp -> FadingIn -> Idle
//
// FadingOut ramps the black overlay (ModuleUI::renderTransitionFade) to 1. HoldBlack waits one whole frame at
// full black, so the frame on screen during the load is a black one. Loading swaps the scene in one blocking
// call. WarmUp keeps the screen black while the new level spawns its player, camera and HUD (the level calls
// NotifyTransitionReady) and a few frames render behind the overlay; FadingIn ramps it back to 0 and releases
// player input.
//
// RuntimeCore::tick() calls update() once per frame, after the scene update and before anything is recorded
// for rendering, so the swap always happens at a frame boundary - including when the request came from a
// script's trigger callback. Timings: Assets/Settings/scene_transition.json (defaults below if absent), re-read
// at the start of every transition. Load and warm-up times go to the log and to scene_transitions.log in the
// working directory.
//
// Engine-only (Engine and Player projects), never linked into GameScript.dll.
class SceneTransition {
public:
    // Returns true on the tick that loaded the new scene: the next frame's delta includes the load.
    bool update(SceneManager& sm, RuntimeCore& rc);

private:
    using Clock = std::chrono::steady_clock;

    struct Config {
        float fadeOutSeconds = 0.4f;
        float fadeInSeconds = 0.4f;
        int warmUpFrames = 3;             // frames rendered behind the overlay after the level reports ready
        float minBlackSeconds = 0.3f;     // shortest time the screen stays fully black
        float warmUpTimeoutSeconds = 3.f; // fade in anyway if the level never reports ready
    };

    void loadConfig();
    void begin(SceneManager& sm);
    void runLoad(SceneManager& sm, RuntimeCore& rc);
    void beginFadeIn(SceneManager& sm, bool timedOut);
    void finish(SceneManager& sm);

    Config m_cfg;
    std::string m_from;
    std::string m_to;
    Clock::time_point m_requestTime;
    Clock::time_point m_phaseStart;
    Clock::time_point m_blackStart;
    Clock::time_point m_lastWarmTick;
    int m_warmFrames = 0;           // WarmUp ticks so far
    int m_readyFrames = 0;          // WarmUp ticks since the level reported ready
    double m_loadMs = 0.0;
    double m_firstWarmFrameMs = 0.0;// the frame right after the load: lazy mesh uploads, prefab spawns
    size_t m_workingSetBefore = 0;
    double m_longestLoadMs = 0.0;   // over this session
};
