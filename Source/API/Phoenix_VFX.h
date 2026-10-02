#pragma once
#include "API/Phoenix_Types.h"
#include <string>

class GameObject;

namespace Phoenix {

// Settings for a particle layer (ComponentParticleSystem). Plain data defined only here, so the same layout is
// seen by the engine and by GameScript.dll.
struct VfxParticleSettings {
    int   maxParticles = 64;
    float emissionRate = 0.f;        // per second while playing; 0 = bursts only
    bool  looping = false;
    float duration = 0.2f;           // emission time of a non-looping system
    int   shape = 2;                 // 0 point, 1 box, 2 sphere, 3 cone (opens along local +Y)
    float shapeRadius = 0.1f;
    float coneAngleDeg = 25.f;
    bool  worldSpace = true;
    Vec2  life = Vec2(0.3f, 0.6f);   // random range per particle (seconds)
    Vec2  speed = Vec2(2.f, 5.f);
    Vec2  size = Vec2(0.05f, 0.1f);
    Vec2  rotation = Vec2(0.f, 360.f);
    Vec4  startColor = Vec4(1.f, 1.f, 1.f, 1.f);
    Vec4  endColor = Vec4(1.f, 1.f, 1.f, 0.f);
    float startSizeMul = 1.f;
    float endSizeMul = 1.f;
    Vec3  gravity = Vec3(0.f, 0.f, 0.f);
    float turbulence = 0.f;          // flow-field strength; 0 = off
    float turbulenceFrequency = 0.5f;
    bool  gpu = false;               // GPU batch path (ParticlePass, 32 emitters): dense motes, no stretch/premultiplied
    std::string texture;
    int   sheetColumns = 1;
    int   sheetRows = 1;
    int   sheetMode = 0;             // 0 fixed (tile 0 or random), 1 over lifetime, 2 frames per second
    float sheetFps = 12.f;
    bool  sheetLoop = true;
    bool  randomFrame = false;
    int   blend = 1;                 // 0 alpha, 1 additive, 2 premultiplied (CPU path)
    float velocityStretch = 0.f;     // CPU path: sprite length x (1 + stretch x speed)
    int   layer = 0;
};

// Runtime VFX building blocks for the GameScript VFX manager (GameScript/VFX). Everything works on plain
// GameObjects: create one (CreateObject), give it one effect component (Add*), then drive it with the setters.
// All calls are data-only on the engine's components; a null or wrong object is ignored.
struct VFX {
    // ---- objects
    // A new, empty GameObject under `parent` (scene root when null). Safe during a script's Update.
    static GameObject* CreateObject(const std::string& name, GameObject* parent = nullptr);
    static void SetParent(GameObject* go, GameObject* parent);   // keeps the local transform
    // Each adds the component if the object doesn't have one yet; false when the engine can't (no scene/hooks).
    static bool AddParticles(GameObject* go);
    static bool AddDecal(GameObject* go);
    static bool AddPointLight(GameObject* go);
    static bool AddTrail(GameObject* go);
    static bool AddBillboard(GameObject* go);
    // Gives the object a ComponentMesh showing an imported model (e.g. "Assets/.../PH_Sphere_fx.gltf").
    static bool LoadMesh(GameObject* go, const std::string& modelAssetPath);

    // ---- particles
    static void ConfigureParticles(GameObject* go, const VfxParticleSettings& s);
    static void PlayParticles(GameObject* go, bool restart = true);   // restart: the emission clock starts again
    static void StopParticles(GameObject* go, bool clearLive = false);
    static void BurstParticles(GameObject* go, int count);            // spawned on the system's next update
    static void SetParticleColors(GameObject* go, Vec4 startColor, Vec4 endColor);
    static void SetParticleRate(GameObject* go, float perSecond);
    static int  LiveParticles(GameObject* go);

    // ---- decals: the decal projects along the object's local Z through a unit box (scale = size, Z = depth),
    // so a ground decal is rotated -90 deg around X.
    static void SetDecal(GameObject* go, const std::string& texture, Vec3 colour, float opacity,
                         float emissive = 0.f, float albedoMix = 1.f);
    static void SetDecalOpacity(GameObject* go, float opacity);   // <= 0 disables it

    // ---- point lights. transient = one of the 4 VFX slots gathered before the level's lights (never shadowed).
    static void SetPointLight(GameObject* go, Vec3 colour, float intensity, float radius, bool transient = true);
    static void SetLightEnabled(GameObject* go, bool enabled);

    // ---- trails (blend: 0 alpha, 1 additive)
    static void SetTrail(GameObject* go, const std::string& texture, Vec4 startColor, Vec4 endColor,
                         float width, float duration, int blend = 1);
    static void SetTrailEmitting(GameObject* go, bool emitting, bool clear = false);

    // ---- billboards (alignment: 0 screen, 1 world-up, 2 axial)
    static void SetBillboard(GameObject* go, const std::string& texture, Vec2 size, Vec4 tint,
                             int sheetColumns = 1, int sheetRows = 1, float fps = 0.f, bool loop = true, int alignment = 0);
    static void SetBillboardTint(GameObject* go, Vec4 tint);
    static void SetBillboardSize(GameObject* go, Vec2 size);

    // ---- per-object mesh overrides (every ComponentMesh at or under `go` when recursive). Runtime only.
    // tint multiplies base colour; alpha < 1 draws the mesh in the transparent forward pass.
    static void SetMeshTint(GameObject* go, Vec4 tint, bool recursive = true);
    // Replaces the material's base colour and alpha (w <= 0 = back to the material's). For VFX meshes such as a
    // bubble or a beam, whose placeholder material has the wrong colour.
    static void SetMeshBaseColor(GameObject* go, Vec4 color, bool recursive = true);
    // colour added as emissive; rimPower 0 = flat over the surface, > 0 = fresnel rim (2-4 reads as a rim).
    static void SetMeshGlow(GameObject* go, Vec3 colour, float rimPower = 0.f, bool recursive = true);
    static void SetMeshUVScroll(GameObject* go, Vec2 offset, Vec2 unitsPerSecond, bool recursive = true);
    static void ClearMeshFx(GameObject* go, bool recursive = true);

    // ---- time. The scale applies to the scene, scripts, collision and animation from the next frame; it returns
    // to 1 on scene load and when Play stops. HUD, camera shake and hit-stop timers use the unscaled delta.
    static void  SetTimeScale(float scale);
    static float GetTimeScale();
    static float UnscaledDeltaTime();
    static float UnscaledTime();

    static constexpr int kMaxTransientLights = 4;
};

} // namespace Phoenix
