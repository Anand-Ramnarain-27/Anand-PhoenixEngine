#include "Globals.h"
#include "DepthStencilDesc.h"
#include "ModuleDSDescriptors.h"
#include "Application.h"

ModuleDSDescriptors* DepthStencilDesc::getModule()
{
    return app->getDSDescriptors();
}