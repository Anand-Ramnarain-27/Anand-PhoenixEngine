#include "Globals.h"
#include "API/Phoenix_VFX.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "SceneManager.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentParticleSystem.h"
#include "ComponentDecal.h"
#include "ComponentLights.h"
#include "ComponentTrail.h"
#include "ComponentBillboard.h"
#include "ComponentMesh.h"
#include "VfxGuards.h"
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <set>
#include <string>
#include <utility>

// Core-side (links into PhoenixCore.lib / GameScript.dll): data-only writes to components the engine owns, reached
// through `app`. Creating a component or loading a model needs engine-executable code, so those go through the
// EngineHooks function pointers RuntimeCore::init registers on the SceneManager.

void VfxGuards::RejectOnce(const GameObject* go, const char* what, const char* detailFmt, ...){
    static std::mutex mtx;
    static std::set<std::pair<uint32_t, std::string>> seen;
    const uint32_t uid = go ? go->getUID() : 0u;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!seen.emplace(uid, what ? what : "").second) return;
    }
    char detail[256] = {};
    va_list args;
    va_start(args, detailFmt);
    vsnprintf(detail, sizeof(detail), detailFmt, args);
    va_end(args);
    char line[512] = {};
    snprintf(line, sizeof(line), "[VFX] rejected %s %s on %s (logged once per object and kind)",
             what ? what : "value", detail, go ? go->getName().c_str() : "<null>");
    PHX_LOG(Script, Info, "%s", line);
    SceneManager* sm = app && app->getRuntimeCore() ? app->getRuntimeCore()->getSceneManager() : nullptr;
    if (sm && sm->getEngineHooks().logWarning) sm->getEngineHooks().logWarning(line);
}

bool VfxGuards::SanitizeDecalWorld(const GameObject* go, Matrix& world){
    if (!finite(world)){
        RejectOnce(go, "decal matrix", "(non-finite)");
        return false;
    }
    // Rows 0-2 are the box's local X/Y/Z axes times the object's scale.
    for (int r = 0; r < 3; ++r){
        Vector3 axis(world.m[r][0], world.m[r][1], world.m[r][2]);
        const float len = axis.Length();
        if (len >= kMinDecalScale) continue;
        if (len < 1e-12f){
            RejectOnce(go, "decal scale", "(axis %d is zero)", r);
            return false;
        }
        RejectOnce(go, "decal scale", "%.5f on axis %d (clamped to %.2f)", len, r, kMinDecalScale);
        axis *= kMinDecalScale / len;
        world.m[r][0] = axis.x; world.m[r][1] = axis.y; world.m[r][2] = axis.z;
    }
    return true;
}

namespace Phoenix {

namespace {
    SceneManager* sceneManager(){
        if (!app || !app->getRuntimeCore()) return nullptr;
        return app->getRuntimeCore()->getSceneManager();
    }

    bool addComponent(GameObject* go, Component::Type type){
        SceneManager* sm = sceneManager();
        if (!go || !sm || !sm->getEngineHooks().addComponent) return false;
        return sm->getEngineHooks().addComponent(go, (int)type) != nullptr;
    }

    template<class T> T* comp(GameObject* go){ return go ? go->getComponent<T>() : nullptr; }

    void forEachMesh(GameObject* go, bool recursive, const std::function<void(ComponentMesh&)>& fn){
        if (!go) return;
        if (auto* cm = go->getComponent<ComponentMesh>()) fn(*cm);
        if (!recursive) return;
        for (GameObject* c : go->getChildren()) forEachMesh(c, true, fn);
    }
}

GameObject* VFX::CreateObject(const std::string& name, GameObject* parent){
    if (!app || !app->getRuntimeCore()) return nullptr;
    SceneGraph* sg = app->getRuntimeCore()->getActiveModuleScene();
    return sg ? sg->createGameObject(name, parent) : nullptr;
}

void VFX::SetParent(GameObject* go, GameObject* parent){
    if (!go) return;
    if (!parent){
        if (SceneGraph* sg = app && app->getRuntimeCore() ? app->getRuntimeCore()->getActiveModuleScene() : nullptr)
            parent = sg->getRoot();
    }
    if (parent && go->getParent() != parent) go->setParent(parent);
}

bool VFX::AddParticles(GameObject* go){ return addComponent(go, Component::Type::ParticleSystem); }
bool VFX::AddDecal(GameObject* go){ return addComponent(go, Component::Type::Decal); }
bool VFX::AddPointLight(GameObject* go){ return addComponent(go, Component::Type::PointLight); }
bool VFX::AddTrail(GameObject* go){ return addComponent(go, Component::Type::Trail); }
bool VFX::AddBillboard(GameObject* go){ return addComponent(go, Component::Type::Billboard); }

bool VFX::PlaceObject(GameObject* go, Vec3 position, Quat rotation, Vec3 scale){
    ComponentTransform* t = go ? go->getTransform() : nullptr;
    if (!t) return false;
    if (!VfxGuards::finite(position) || !VfxGuards::finite(rotation) || !VfxGuards::finite(scale)){
        VfxGuards::RejectOnce(go, "transform", "(non-finite position/rotation/scale)");
        return false;
    }
    t->position = position;
    t->rotation = rotation;
    t->scale = scale;
    t->markDirty();
    return true;
}

bool VFX::LoadMesh(GameObject* go, const std::string& modelAssetPath){
    SceneManager* sm = sceneManager();
    if (!go || !sm || !sm->getEngineHooks().loadModel) return false;
    return sm->getEngineHooks().loadModel(go, modelAssetPath.c_str());
}

void VFX::ConfigureParticles(GameObject* go, const VfxParticleSettings& s){
    auto* ps = comp<ComponentParticleSystem>(go);
    if (!ps) return;
    ps->enabled = true;
    ps->maxParticles = std::max(1, s.maxParticles);
    ps->emissionRate = std::max(0.f, s.emissionRate);
    ps->looping = s.looping;
    ps->duration = std::max(0.01f, s.duration);
    ps->shape = (ComponentParticleSystem::EmitterShape)std::clamp(s.shape, 0, 3);
    ps->shapeRadius = std::max(0.f, s.shapeRadius);
    ps->coneAngleDeg = s.coneAngleDeg;
    ps->directionMode = s.direction == 1 ? ComponentParticleSystem::DirectionMode::Radial
                                         : ComponentParticleSystem::DirectionMode::Random;
    ps->worldSpace = s.worldSpace;
    ps->lifeRange = s.life;
    ps->speedRange = s.speed;
    ps->sizeRange = s.size;
    ps->rotationRange = s.rotation;
    ps->startColor = s.startColor;
    ps->endColor = s.endColor;
    ps->startSizeMul = s.startSizeMul;
    ps->endSizeMul = s.endSizeMul;
    ps->gravity = s.gravity;
    ps->useTurbulence = s.turbulence > 0.f;
    ps->turbulenceStrength = s.turbulence;
    ps->turbulenceFrequency = s.turbulenceFrequency;
    ps->useGPU = s.gpu;
    ps->texturePath = s.texture;
    ps->sheetColumns = std::max(1, s.sheetColumns);
    ps->sheetRows = std::max(1, s.sheetRows);
    ps->sheetMode = (ComponentParticleSystem::SheetMode)std::clamp(s.sheetMode, 0, 2);
    ps->sheetFps = s.sheetFps;
    ps->sheetLoop = s.sheetLoop;
    ps->randomFrame = s.randomFrame;
    ps->blendMode = (ComponentParticleSystem::BlendMode)std::clamp(s.blend, 0, 2);
    ps->velocityStretch = std::max(0.f, s.velocityStretch);
    ps->layer = s.layer;
}

void VFX::PlayParticles(GameObject* go, bool restart){
    if (auto* ps = comp<ComponentParticleSystem>(go)){
        ps->enabled = true;
        if (restart) ps->restart(); else ps->play();
    }
}

void VFX::StopParticles(GameObject* go, bool clearLive){
    if (auto* ps = comp<ComponentParticleSystem>(go)){
        ps->stop();
        ps->pendingBurst = 0;
        if (clearLive) ps->clear();
    }
}

void VFX::BurstParticles(GameObject* go, int count){
    if (auto* ps = comp<ComponentParticleSystem>(go); ps && count > 0){
        ps->enabled = true;
        ps->pendingBurst += count;
    }
}

void VFX::SetParticleColors(GameObject* go, Vec4 startColor, Vec4 endColor){
    if (auto* ps = comp<ComponentParticleSystem>(go)){ ps->startColor = startColor; ps->endColor = endColor; }
}

void VFX::SetParticleRate(GameObject* go, float perSecond){
    if (auto* ps = comp<ComponentParticleSystem>(go)) ps->emissionRate = std::max(0.f, perSecond);
}

int VFX::LiveParticles(GameObject* go){
    auto* ps = comp<ComponentParticleSystem>(go);
    return ps ? ps->aliveCount() + ps->pendingBurst : 0;
}

void VFX::SetDecal(GameObject* go, const std::string& texture, Vec3 colour, float opacity, float emissive, float albedoMix){
    if (auto* dc = comp<ComponentDecal>(go)){
        if (!VfxGuards::finite(colour) || !VfxGuards::finite(opacity) || !VfxGuards::finite(emissive) ||
            !VfxGuards::finite(albedoMix)){
            VfxGuards::RejectOnce(go, "decal params", "(non-finite colour/opacity/emissive/albedoMix)");
            dc->opacity = 0.f;
            dc->enabled = false;
            return;
        }
        dc->texturePath = texture;
        dc->colour = colour;
        dc->opacity = std::clamp(opacity, 0.f, 1.f);
        dc->emissive = std::max(0.f, emissive);
        dc->albedoMix = std::clamp(albedoMix, 0.f, 1.f);
        dc->enabled = dc->opacity > 0.f;
    }
}

void VFX::SetDecalOpacity(GameObject* go, float opacity){
    if (auto* dc = comp<ComponentDecal>(go)){
        if (!VfxGuards::finite(opacity)){
            VfxGuards::RejectOnce(go, "decal opacity", "(non-finite)");
            opacity = 0.f;
        }
        dc->opacity = std::clamp(opacity, 0.f, 1.f);
        dc->enabled = dc->opacity > 0.f;
    }
}

bool VFX::PlaceDecal(GameObject* go, Vec3 position, Quat rotation, Vec3 size){
    ComponentTransform* t = go ? go->getTransform() : nullptr;
    if (!t) return false;
    auto* dc = comp<ComponentDecal>(go);
    if (!VfxGuards::finite(position) || !VfxGuards::finite(rotation) || !VfxGuards::finite(size)){
        VfxGuards::RejectOnce(go, "decal pose", "(non-finite position/rotation/size)");
        if (dc){ dc->opacity = 0.f; dc->enabled = false; }
        return false;
    }
    const float m = VfxGuards::kMinDecalScale;
    if (std::fabs(size.x) < m || std::fabs(size.y) < m || std::fabs(size.z) < m){
        VfxGuards::RejectOnce(go, "decal scale", "(%.4f, %.4f, %.4f) (clamped to %.2f)", size.x, size.y, size.z, m);
        size = Vec3((std::max)(std::fabs(size.x), m), (std::max)(std::fabs(size.y), m), (std::max)(std::fabs(size.z), m));
    }
    t->position = position;
    t->rotation = rotation;
    t->scale = size;
    t->markDirty();
    return true;
}

void VFX::SetPointLight(GameObject* go, Vec3 colour, float intensity, float radius, bool transient){
    if (auto* pl = comp<ComponentPointLight>(go)){
        if (!VfxGuards::finite(colour) || !VfxGuards::finite(intensity) || !VfxGuards::finite(radius)){
            VfxGuards::RejectOnce(go, "light params", "(non-finite colour/intensity/radius)");
            pl->intensity = 0.f;
            pl->enabled = false;
            return;
        }
        pl->color = colour;
        pl->intensity = std::max(0.f, intensity);
        pl->radius = std::max(0.01f, radius);
        pl->transient = transient;
        if (transient) pl->castShadows = false;
    }
}

void VFX::SetLightEnabled(GameObject* go, bool enabled){
    if (auto* pl = comp<ComponentPointLight>(go)) pl->enabled = enabled;
}

void VFX::SetTrail(GameObject* go, const std::string& texture, Vec4 startColor, Vec4 endColor,
                   float width, float duration, int blend){
    if (auto* tr = comp<ComponentTrail>(go)){
        tr->enabled = true;
        tr->texturePath = texture;
        tr->startColor = startColor;
        tr->endColor = endColor;
        tr->width = std::max(0.001f, width);
        tr->duration = std::max(0.01f, duration);
        tr->blendMode = blend == 1 ? ComponentTrail::BlendMode::Additive : ComponentTrail::BlendMode::Alpha;
        tr->previewOrbit = false;
    }
}

void VFX::SetTrailEmitting(GameObject* go, bool emitting, bool clear){
    if (auto* tr = comp<ComponentTrail>(go)){
        tr->emitting = emitting;
        if (clear) tr->clear();
    }
}

void VFX::SetBillboard(GameObject* go, const std::string& texture, Vec2 size, Vec4 tint,
                       int sheetColumns, int sheetRows, float fps, bool loop, int alignment){
    if (auto* bb = comp<ComponentBillboard>(go)){
        bb->enabled = true;
        bb->texturePath = texture;
        bb->size = size;
        bb->tint = tint;
        bb->sheetColumns = std::max(1, sheetColumns);
        bb->sheetRows = std::max(1, sheetRows);
        bb->framesPerSecond = std::max(0.f, fps);
        bb->loop = loop;
        bb->alignment = (ComponentBillboard::Alignment)std::clamp(alignment, 0, 2);
    }
}

void VFX::SetBillboardTint(GameObject* go, Vec4 tint){
    if (!VfxGuards::finite(tint)){ VfxGuards::RejectOnce(go, "billboard tint", "(non-finite)"); return; }
    if (auto* bb = comp<ComponentBillboard>(go)){ bb->tint = tint; bb->enabled = tint.w > 0.001f; }
}

void VFX::SetBillboardSize(GameObject* go, Vec2 size){
    if (!VfxGuards::finite(size)){ VfxGuards::RejectOnce(go, "billboard size", "(non-finite)"); return; }
    if (auto* bb = comp<ComponentBillboard>(go)) bb->size = size;
}

void VFX::SetMeshTint(GameObject* go, Vec4 tint, bool recursive){
    if (!VfxGuards::finite(tint)){ VfxGuards::RejectOnce(go, "mesh tint", "(non-finite)"); return; }
    forEachMesh(go, recursive, [&](ComponentMesh& cm){ cm.vfx.tint = tint; });
}

void VFX::SetMeshBaseColor(GameObject* go, Vec4 color, bool recursive){
    if (!VfxGuards::finite(color)){ VfxGuards::RejectOnce(go, "mesh base colour", "(non-finite)"); return; }
    forEachMesh(go, recursive, [&](ComponentMesh& cm){ cm.vfx.baseColor = color; });
}

void VFX::SetMeshGlow(GameObject* go, Vec3 colour, float rimPower, bool recursive){
    if (!VfxGuards::finite(colour) || !VfxGuards::finite(rimPower)){ VfxGuards::RejectOnce(go, "mesh glow", "(non-finite)"); return; }
    forEachMesh(go, recursive, [&](ComponentMesh& cm){
        cm.vfx.emissive = Vector4(colour.x, colour.y, colour.z, std::max(0.f, rimPower));
    });
}

void VFX::SetMeshUVScroll(GameObject* go, Vec2 offset, Vec2 unitsPerSecond, bool recursive){
    if (!VfxGuards::finite(offset) || !VfxGuards::finite(unitsPerSecond)){ VfxGuards::RejectOnce(go, "mesh UV scroll", "(non-finite)"); return; }
    forEachMesh(go, recursive, [&](ComponentMesh& cm){ cm.vfx.uvOffset = offset; cm.vfx.uvScroll = unitsPerSecond; });
}

void VFX::ClearMeshFx(GameObject* go, bool recursive){
    forEachMesh(go, recursive, [](ComponentMesh& cm){ cm.vfx = MeshVfxParams(); });
}

void VFX::SetTimeScale(float scale){
    if (!VfxGuards::finite(scale)) scale = 1.f;
    if (SceneManager* sm = sceneManager()) sm->getRuntimeTime().timeScale = std::clamp(scale, 0.f, 4.f);
}

float VFX::GetTimeScale(){
    SceneManager* sm = sceneManager();
    return sm ? sm->getRuntimeTime().timeScale : 1.f;
}

float VFX::UnscaledDeltaTime(){
    SceneManager* sm = sceneManager();
    return sm ? sm->getRuntimeTime().unscaledDeltaTime : 0.f;
}

float VFX::UnscaledTime(){
    SceneManager* sm = sceneManager();
    return sm ? sm->getRuntimeTime().unscaledTime : 0.f;
}

} // namespace Phoenix
