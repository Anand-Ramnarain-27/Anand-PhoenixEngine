// PhoenixCore's stub for CreateNonAnimationResource() (declared in
// ModuleResources.h) - GameScript.dll only ever requests Animation resources
// today (via ComponentAnimation::SendTrigger -> pushLayer), which
// ModuleResourcesCore.cpp's CreateResourceFromUID() handles inline without
// calling this at all. This stub exists purely to satisfy the linker if that
// ever changes; see ModuleResourcesFactories.cpp for the real implementation
// Engine/Player link instead.
#include "Globals.h"
#include "ModuleResources.h"

ResourceBase* CreateNonAnimationResource(ResourceBase::Type, UID, const std::string&, UID, const std::string&){
    return nullptr;
}
