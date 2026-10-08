#include "Globals.h"
#include "Application.h"
#include "ModuleInput.h"
#include "API/Phoenix_Time.h"
#include "ModuleD3D12.h"
#include "ModuleFileSystem.h"
#include "ModuleGPUResources.h"
#include "ModuleResources.h"
#include "ModuleSamplerHeap.h"
#include "ModuleCamera.h"
#ifdef PHOENIX_EDITOR
#include "ModuleEditor.h"
#endif
#include "RuntimeCore.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleRingBuffer.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleStaticBuffer.h"
#include "ModuleAssets.h"
#include "ModuleUI.h"

namespace {
uint64_t nowMilis(){
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
}

Application::Application(int argc, wchar_t** argv, void* hWnd){
    m_modules.push_back(m_fileSystem = new ModuleFileSystem());
    m_modules.push_back(m_input = new ModuleInput((HWND)hWnd));
    m_modules.push_back(m_d3d12 = new ModuleD3D12((HWND)hWnd));
    m_modules.push_back(m_gpuResources = new ModuleGPUResources());
    m_modules.push_back(m_resources = new ModuleResources());
    m_modules.push_back(m_samplerHeaps = new ModuleSamplerHeap());
    m_modules.push_back(m_camera = new ModuleCamera());
    m_modules.push_back(m_shaderDescriptors = new ModuleShaderDescriptors());
    m_modules.push_back(m_rtDescriptors = new ModuleRTDescriptors());
    m_modules.push_back(m_dsDescriptors = new ModuleDSDescriptors());
    m_modules.push_back(m_ringBuffer = new ModuleRingBuffer());
    m_modules.push_back(m_assets = new ModuleAssets());
    m_modules.push_back(m_ui = new ModuleUI());

#ifdef PHOENIX_EDITOR
    m_modules.push_back(m_runtimeCore = new RuntimeCore(/*standalone=*/false));
    m_modules.push_back(m_editor = new ModuleEditor());
#else
    m_modules.push_back(m_runtimeCore = new RuntimeCore(/*standalone=*/true));
#endif

    m_staticBuffer = new ModuleStaticBuffer();
}

Application::~Application(){
    cleanUp();

    for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it){
        delete *it;
    }
}

bool Application::init(){
    bool ret = true;

    for (auto it = m_modules.begin(); it != m_modules.end() && ret; ++it)
        ret = (*it)->init();

    m_lastMilis = nowMilis();

    return ret;
}

void Application::update(){
    if (m_updating) return;
    m_updating = true;

    const uint64_t currentMilis = nowMilis();

    m_elapsedMilis = currentMilis - m_lastMilis;
    m_lastMilis = currentMilis;
    m_tickSum -= m_tickList[m_tickIndex];
    m_tickSum += m_elapsedMilis;
    m_tickList[m_tickIndex] = m_elapsedMilis;
    m_tickIndex = (m_tickIndex + 1) % kFpsTicks;

    Phoenix::Time::deltaTime      = m_elapsedMilis * 0.001f;
    Phoenix::Time::timeSinceStart = currentMilis * 0.001f;
    Phoenix::Time::fps            = getFPS();
    Phoenix::Time::frameCount    += 1;

    if (!m_paused){
        for (Module* module : m_modules) module->update();
        for (Module* module : m_modules) module->preRender();
        for (Module* module : m_modules) module->render();
        for (Module* module : m_modules) module->postRender();
    }

    m_updating = false;
}

bool Application::cleanUp(){
    bool ret = true;

    for (auto it = m_modules.rbegin(); it != m_modules.rend() && ret; ++it)
        ret = (*it)->cleanUp();

    return ret;
}
