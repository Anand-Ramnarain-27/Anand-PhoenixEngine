#include "Globals.h"
#include "Application.h"
#include "ModuleInput.h"

#include "ModuleD3D12.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"
#include "ModuleSamplerHeap.h"
#include "ModuleCamera.h"
#include "ModuleEditor.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleRingBuffer.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "RenderTexture.h"
#include "ModuleAssets.h"

//#include "ModuleTextureSampler.h"
//#include "BasicModelScene.h"
//#include "LightingDemo.h"
//#include "RenderToTextureDemo.h"
//#include "ViewportDemo.h"

Application::Application(int argc, wchar_t** argv, void* hWnd)
{
    modules.push_back(fileSystem = new ModuleFileSystem());
    modules.push_back(new ModuleInput((HWND)hWnd));
    modules.push_back(d3d12Module = new ModuleD3D12((HWND)hWnd));
    modules.push_back(resources = new ModuleResources());
    modules.push_back(samplerHeaps = new ModuleSamplerHeap());
	modules.push_back(camera = new ModuleCamera());
    modules.push_back(shaderDescriptors = new ModuleShaderDescriptors());
    modules.push_back(rtDescriptors = new ModuleRTDescriptors());
    modules.push_back(dsDescriptors = new ModuleDSDescriptors());
	modules.push_back(ringBuffer = new ModuleRingBuffer());
	modules.push_back(assets = new ModuleAssets());
    modules.push_back(editor = new ModuleEditor());

	//modules.push_back(basicModelScene = new BasicModelScene());
	//modules.push_back(lightingDemo = new LightingDemo());
	//modules.push_back(renderToTextureDemo = new RenderToTextureDemo());
	//modules.push_back(viewportDemo = new ViewportDemo());

	/*modules.push_back(textureSampler = new ModuleTextureSampler());*/
}

Application::~Application()
{
    cleanUp();

	for(auto it = modules.rbegin(); it != modules.rend(); ++it)
    {
        delete *it;
    }
}
 
bool Application::init()
{
	bool ret = true;

	for(auto it = modules.begin(); it != modules.end() && ret; ++it)
		ret = (*it)->init();

    lastMilis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	return ret;
}

void Application::update()
{
    using namespace std::chrono_literals;

    if (!updating)
    {
        updating = true;

        uint64_t currentMilis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        elapsedMilis = currentMilis - lastMilis;
        lastMilis = currentMilis;
        tickSum -= tickList[tickIndex];
        tickSum += elapsedMilis;
        tickList[tickIndex] = elapsedMilis;
        tickIndex = (tickIndex + 1) % MAX_FPS_TICKS;

        if (!app->paused)
        {
            for (auto it = swapModules.begin(); it != swapModules.end(); ++it)
            {
                auto pos = std::find(modules.begin(), modules.end(), it->first);
                if (pos != modules.end())
                {
                    (*pos)->cleanUp();
                    delete* pos;

                    it->second->init();
                    *pos = it->second;
                }
            }

            swapModules.clear();

            for (auto it = modules.begin(); it != modules.end(); ++it)
                (*it)->update();

            for (auto it = modules.begin(); it != modules.end(); ++it)
                (*it)->preRender();

            for (auto it = modules.begin(); it != modules.end(); ++it)
                (*it)->render();

            for (auto it = modules.begin(); it != modules.end(); ++it)
                (*it)->postRender();
        }

        updating = false;
    }
}

bool Application::cleanUp()
{
	bool ret = true;

	for(auto it = modules.rbegin(); it != modules.rend() && ret; ++it)
		ret = (*it)->cleanUp();

	return ret;
}
