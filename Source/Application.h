#pragma once

#include "Globals.h"

#include <array>
#include <vector>
#include <chrono>

class Module;
class ModuleD3D12;
class ModuleResources;
class GraphicsSamplers;
class ModuleCamera;
class ModuleEditor;
class ModuleTextureSampler;
class ModuleShaderDescriptors;
class ModuleModelViewer;

class Application
{
public:

	Application(int argc, wchar_t** argv, void* hWnd);
	~Application();

	bool         init();
	void         update();
	bool         cleanUp();

    ModuleD3D12* getD3D12() { return d3d12Module; }
    ModuleResources* getResources() { return resources; }
    GraphicsSamplers* getGraphicsSamplers() { return graphicsSamplers; }
	ModuleCamera* getCamera() { return camera; }
    ModuleEditor* getEditor() { return editor; }
	ModuleTextureSampler* getTextureSampler() { return textureSampler; }
    ModuleShaderDescriptors* getShaderDescriptors() { return shaderDescriptors; }
	ModuleModelViewer* getModelViewer() { return modelViewer; }
    void                        swapModule(Module* from, Module* to) { swapModules.push_back(std::make_pair(from, to)); }
    
    float                       getFPS() const { return 1000.0f * float(MAX_FPS_TICKS) / tickSum; }
    float                       getAvgElapsedMs() const { return tickSum / float(MAX_FPS_TICKS); }
    uint64_t                    getElapsedMilis() const { return elapsedMilis; }

    bool                        isPaused() const { return paused; }
    bool                        setPaused(bool p) { paused = p; return paused; }

private:
    enum { MAX_FPS_TICKS = 30 };
    typedef std::array<uint64_t, MAX_FPS_TICKS> TickList;

    std::vector<Module*> modules;
    std::vector<std::pair<Module*, Module*> > swapModules;

    ModuleD3D12* d3d12Module = nullptr;
    ModuleResources* resources = nullptr;
    GraphicsSamplers* graphicsSamplers = nullptr;
    ModuleCamera* camera = nullptr;
	ModuleEditor* editor = nullptr;
	ModuleTextureSampler* textureSampler = nullptr;
    ModuleShaderDescriptors* shaderDescriptors = nullptr;
	ModuleModelViewer* modelViewer = nullptr;

    uint64_t  lastMilis = 0;
    TickList  tickList;
    uint64_t  tickIndex;
    uint64_t  tickSum = 0;
    uint64_t  elapsedMilis = 0;
    bool      paused = false;
    bool      updating = false;
};

extern Application* app;
