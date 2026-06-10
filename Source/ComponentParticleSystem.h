#pragma once
#include "Component.h"
#include "Globals.h"
#include "ShaderTableDesc.h"
#include "CurveWidget.h"
#include <vector>
#include <random>
#include <d3d12.h>
#include <wrl/client.h>

// CPU particle system component.
// An emitter spawns particles at a configurable rate from a shape (cone, box or
// sphere). Each particle gets randomised initial position/speed/size/rotation/
// life/colour, then decays toward an "end" size multiplier and colour over its
// lifetime via linear interpolation over-time curves. Rendering reuses the existing
// billboard pipeline — every
// live particle is converted into a camera-facing BillboardInstance each frame.
class ComponentParticleSystem : public Component {
public:
    enum class EmitterShape {
        Point = 0,
        Box = 1,
        Sphere = 2,
        Cone = 3,
    };

    enum class BlendMode {
        Alpha = 0, // src*srcAlpha + dst*(1-srcAlpha) — smoke / glow edges
        Additive = 1, // src + dst — fire / lights / sparks
    };

    explicit ComponentParticleSystem(GameObject* owner);
    ~ComponentParticleSystem() override = default;

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::ParticleSystem; }

    // ---- Emitter ----
    bool enabled = true;
    bool playing = true;
    bool looping = true;
    float duration = 5.f; // seconds; only relevant when !looping
    float emissionRate = 20.f; // particles spawned per second
    int maxParticles = 256;

    EmitterShape shape = EmitterShape::Cone;
    float shapeRadius = 0.5f; // Box: half-extent: Sphere: radius; Cone: base radius
    float coneAngleDeg = 25.f; // Cone: half-angle of the spread

    bool worldSpace = true; // spawned particles ignore later emitter movement

    // ---- Initial particle values (random ranges, min..max) ----
    Vector2 lifeRange = Vector2(1.f, 2.f);
    Vector2 speedRange = Vector2(1.f, 3.f);
    Vector2 sizeRange = Vector2(0.25f, 0.5f);
    Vector2 rotationRange = Vector2(-45.f, 45.f); // degrees, initial billboard rotation

    Vector4 startColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 endColor = Vector4(1.f, 1.f, 1.f, 0.f);

    // Size multiplier applied over lifetime: lerp(startSizeMul, endSizeMul, sizeCurve.Eval(t))
    float startSizeMul = 1.f;
    float endSizeMul = 1.f;
    EaseCurve sizeCurve;

    Vector3 gravity = Vector3(0.f, 0.f, 0.f);

    // ---- Turbulence / flow-field noise ----
    // A 3D fractal gradient noise sampled at the particle's world position is
    // used as a polar angle to build a flow vector over the XZ plane:
    //   angle = clamp(noise*0.5+0.5, 0, 1) * 2*pi
    //   flow  = (cos(angle), 0, sin(angle))
    // A second noise sample (offset from the first) modulates the flow strength,
    // and the result is added to the particle's velocity each frame.
    bool useTurbulence = false;
    float turbulenceFrequency = 0.5f; // noise sampling frequency (world-space)
    float turbulenceStrength = 1.5f; // max added flow speed (m/s)
    int turbulenceOctaves = 3; // fbm octave count
    float turbulenceScroll = 0.3f; // animates the noise field over time (m/s along +Y)

    // ---- GPU rendering / simulation path ----
    // When true the particle pass is handled by ParticlePass (one draw call per
    // emitter, optional GPU turbulence via ParticleUpdateCS) instead of the
    // per-billboard BillboardPass path.  Default false keeps existing behaviour.
    bool useGPU = false;

    // ---- Render ----
    std::string texturePath;
    int sheetColumns = 1;
    int sheetRows = 1;
    bool randomFrame = false; // pick one random sub-image per particle vs. animating
    BlendMode blendMode = BlendMode::Alpha;
    int layer = 0; // higher draws after lower (manual sort tie-break)

    struct Particle {
        Vector3 position;
        Vector3 velocity;
        float rotationDeg = 0.f;
        float baseSize = 1.f;
        float age = 0.f;
        float lifetime = 1.f;
        int frameIndex = 0;
        bool alive = false;
    };

    const std::vector<Particle>& getParticles() const { return m_particles; }

    // Linear interpolation helpers — over-time values for a particle at
    // normalised age t = age/lifetime in [0,1].
    Vector4 colorAt(float t) const{
        return Vector4(startColor.x + (endColor.x - startColor.x) * t,
                       startColor.y + (endColor.y - startColor.y) * t,
                       startColor.z + (endColor.z - startColor.z) * t,
                       startColor.w + (endColor.w - startColor.w) * t);
    }
    float sizeMultiplierAt(float t) const { return startSizeMul + (endSizeMul - startSizeMul) * sizeCurve.Eval(t); }

    void play() { playing = true; }
    void stop() { playing = false; }
    void clear() { m_particles.clear(); m_spawnAccumulator = 0.f; m_age = 0.f; }

private:
    void spawnParticle();
    Vector3 randomEmitDirection(std::mt19937& rng) const;
    Vector3 randomEmitPosition(std::mt19937& rng) const;

    // Regenerates the small grayscale preview texture shown in the Turbulence
    // section, sampling the same fbm noise used by the flow field.
    void updateNoisePreview();

    std::vector<Particle> m_particles;
    float m_spawnAccumulator = 0.f;
    float m_age = 0.f;
    mutable std::mt19937 m_rng{ std::random_device{}() };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_noisePreviewTex;
    ShaderTableDesc m_noisePreviewSRV;
    bool m_noisePreviewDirty = true;
};
