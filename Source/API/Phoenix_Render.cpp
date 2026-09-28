#include "Globals.h"
#include "API/Phoenix_Render.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "SceneManager.h"

// Core-side (links into PhoenixCore.lib / GameScript.dll): only touches inline SceneManager accessors, so a
// script's write lands in the engine's SceneManager instance rather than in state private to the DLL.

namespace Phoenix {

static RenderOverrides* getOverrides(){
    if (!app || !app->getRuntimeCore()) return nullptr;
    SceneManager* sm = app->getRuntimeCore()->getSceneManager();
    return sm ? &sm->getRenderOverrides() : nullptr;
}

void Render::SetOcclusionFade(bool enabled){
    if (RenderOverrides* o = getOverrides()) o->occlusionEnabled = enabled ? 1 : 0;
}

void Render::SetOcclusionFadeRadius(float radius){
    if (RenderOverrides* o = getOverrides()) o->occlusionRadius = radius < 0.f ? 0.f : radius;
}

void Render::SetOcclusionFocusOverride(bool use, Vec3 worldPos){
    if (RenderOverrides* o = getOverrides()){
        o->useFocusOverride = use;
        o->focusOverride = worldPos;
    }
}

void Render::SetXRayEnabled(bool enabled){
    if (RenderOverrides* o = getOverrides()) o->xrayEnabled = enabled ? 1 : 0;
}

void Render::ResetOverrides(){
    if (RenderOverrides* o = getOverrides()) o->reset();
}

} // namespace Phoenix
