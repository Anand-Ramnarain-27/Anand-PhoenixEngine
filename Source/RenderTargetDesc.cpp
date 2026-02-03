#include "Globals.h"
#include "RenderTargetDesc.h"
#include "ModuleRTDescriptors.h"
#include "Application.h"

ModuleRTDescriptors* RenderTargetDesc::getModule()
{
    return app->getRTDescriptors();
}