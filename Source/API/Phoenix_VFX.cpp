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
#include <algorithm>
#include <functional>

// Core-side (links into PhoenixCore.lib / GameScript.dll): data-only writes to components the engine owns, reached
// through `app`. Creating a component or loading a model needs engine-executable code, so those go through the
// EngineHooks function pointers RuntimeCore::init registers on the SceneManager.

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

// ------------------------------------------------------------------ objects

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

bool VFX::LoadMesh(GameObject* go, const std::string& modelAssetPath){
    SceneManager* sm = sceneManager();
    if (!go || !sm || !sm->getEngineHooks().loadModel) return false;
    return sm->getEngineHooks().loadModel(go, modelAssetPath.c_str());
}

// ------------------------------------------------------------------ particles

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

// ------------------------------------------------------------------ decals

void VFX::SetDecal(GameObject* go, const std::string& texture, Vec3 colour, float opacity, float emissive, float albedoMix){
    if (auto* dc = comp<ComponentDecal>(go)){
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
        dc->opacity = std::clamp(opacity, 0.f, 1.f);
        dc->enabled = dc->opacity > 0.f;
    }
}

// ------------------------------------------------------------------ lights

void VFX::SetPointLight(GameObject* go, Vec3 colour, float intensity, float radius, bool transient){
    if (auto* pl = comp<ComponentPointLight>(go)){
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

// ------------------------------------------------------------------ trails

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

// ------------------------------------------------------------------ billboards

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
    if (auto* bb = comp<ComponentBillboard>(go)){ bb->tint = tint; bb->enabled = tint.w > 0.001f; }
}

void VFX::SetBillboardSize(GameObject* go, Vec2 size){
    if (auto* bb = comp<ComponentBillboard>(go)) bb->size = size;
}

// ------------------------------------------------------------------ mesh overrides

void VFX::SetMeshTint(GameObject* go, Vec4 tint, bool recursive){
    forEachMesh(go, recursive, [&](ComponentMesh& cm){ cm.vfx.tint = tint; });
}

void VFX::SetMeshBaseColor(GameObject* go, Vec4 color, bool recursive){
    forEachMesh(go, recursive, [&](ComponentMesh& cm){ cm.vfx.baseColor = color; });
}

void VFX::SetMeshGlow(GameObject* go, Vec3 colour, float rimPower, bool recursive){
    forEachMesh(go, recursive, [&](ComponentMesh& cm){
        cm.vfx.emissive = Vector4(colour.x, colour.y, colour.z, std::max(0.f, rimPower));
    });
}

void VFX::SetMeshUVScroll(GameObject* go, Vec2 offset, Vec2 unitsPerSecond, bool recursive){
    forEachMesh(go, recursive, [&](ComponentMesh& cm){ cm.vfx.uvOffset = offset; cm.vfx.uvScroll = unitsPerSecond; });
}

void VFX::ClearMeshFx(GameObject* go, bool recursive){
    forEachMesh(go, recursive, [](ComponentMesh& cm){ cm.vfx = MeshVfxParams(); });
}

// ------------------------------------------------------------------ time

void VFX::SetTimeScale(float scale){
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
