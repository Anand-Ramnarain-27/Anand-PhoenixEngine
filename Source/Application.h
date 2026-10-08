#pragma once
// The application: owns every engine module and drives them through init, the per-frame update/render cycle and
// shutdown. One instance, reachable everywhere through the global `app`.

#include "Globals.h"
#include "ModuleFileSystem.h"
#include "ModuleInput.h"

#include <array>
#include <vector>
#include <chrono>

class Module;
class ModuleD3D12;
class ModuleGPUResources;
class ModuleResources;
class ModuleSamplerHeap;
class ModuleCamera;
class ModuleEditor;
class ModuleShaderDescriptors;
class ModuleRingBuffer;
class ModuleRTDescriptors;
class ModuleDSDescriptors;
class ModuleAssets;
class ModuleStaticBuffer;
class ModuleUI;
class RuntimeCore;

/// Owns the engine modules and runs them in registration order (cleanup in reverse). The editor build adds
/// ModuleEditor after RuntimeCore; the Player build runs RuntimeCore standalone.
class Application {
public:
    Application(int argc, wchar_t** argv, void* hWnd);
    ~Application();

    bool init();
    /// One frame: update, preRender, render and postRender on every module. Skipped while paused (minimised).
    void update();
    bool cleanUp();

    ModuleInput* getInput(){ return m_input; }
    ModuleD3D12* getD3D12(){ return m_d3d12; }
    ModuleGPUResources* getGPUResources(){ return m_gpuResources; }
    ModuleResources* getResources(){ return m_resources; }
    ModuleSamplerHeap* getSamplerHeap(){ return m_samplerHeaps; }
    ModuleFileSystem* getFileSystem(){ return m_fileSystem; }
    ModuleCamera* getCamera(){ return m_camera; }
    /// Null in the Player build.
    ModuleEditor* getEditor(){ return m_editor; }
    RuntimeCore* getRuntimeCore(){ return m_runtimeCore; }
    ModuleShaderDescriptors* getShaderDescriptors(){ return m_shaderDescriptors; }
    ModuleRingBuffer* getRingBuffer(){ return m_ringBuffer; }
    ModuleRTDescriptors* getRTDescriptors(){ return m_rtDescriptors; }
    ModuleDSDescriptors* getDSDescriptors(){ return m_dsDescriptors; }
    ModuleAssets* getAssets(){ return m_assets; }
    ModuleStaticBuffer* getStaticBuffer(){ return m_staticBuffer; }
    ModuleUI* getUI(){ return m_ui; }

    /// Frames per second averaged over the last kFpsTicks frames.
    float getFPS() const { return 1000.0f * float(kFpsTicks) / float(m_tickSum); }
    float getAvgElapsedMs() const { return float(m_tickSum) / float(kFpsTicks); }
    /// Wall-clock length of the current frame.
    uint64_t getElapsedMilis() const { return m_elapsedMilis; }

    bool isPaused() const { return m_paused; }
    bool setPaused(bool p){ m_paused = p; return m_paused; }

private:
    static constexpr size_t kFpsTicks = 30;

    std::vector<Module*> m_modules;

    ModuleInput* m_input = nullptr;
    ModuleD3D12* m_d3d12 = nullptr;
    ModuleGPUResources* m_gpuResources = nullptr;
    ModuleResources* m_resources = nullptr;
    ModuleSamplerHeap* m_samplerHeaps = nullptr;
    ModuleFileSystem* m_fileSystem = nullptr;
    ModuleCamera* m_camera = nullptr;
    ModuleShaderDescriptors* m_shaderDescriptors = nullptr;
    ModuleRingBuffer* m_ringBuffer = nullptr;
    ModuleRTDescriptors* m_rtDescriptors = nullptr;
    ModuleDSDescriptors* m_dsDescriptors = nullptr;
    ModuleAssets* m_assets = nullptr;
    ModuleUI* m_ui = nullptr;
    ModuleEditor* m_editor = nullptr;
    ModuleStaticBuffer* m_staticBuffer = nullptr;
    RuntimeCore* m_runtimeCore = nullptr;

    uint64_t m_lastMilis = 0;
    std::array<uint64_t, kFpsTicks> m_tickList = {};
    uint64_t m_tickIndex = 0;
    uint64_t m_tickSum = 0;
    uint64_t m_elapsedMilis = 0;
    bool m_paused = false;
    bool m_updating = false;
};

extern Application* app;
