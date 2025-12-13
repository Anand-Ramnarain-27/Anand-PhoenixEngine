#include "Globals.h"
#include "Application.h"
#include "ModuleInput.h"

#include "ModuleD3D12.h"
#include "ModuleResources.h"
#include "GraphicsSamplers.h"
#include "ModuleCamera.h"
#include "ModuleEditor.h"

Application::Application(int argc, wchar_t** argv, void* hWnd)
{
    modules.push_back(new ModuleInput((HWND)hWnd));

    modules.push_back(d3d12Module = new ModuleD3D12((HWND)hWnd));

    modules.push_back(resources = new ModuleResources());
    modules.push_back(graphicsSamplers = new GraphicsSamplers());
	modules.push_back(camera = new ModuleCamera());

	modules.push_back(editor = new ModuleEditor());
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

        // Update milis
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
