#pragma once
#include "Component.h"
#include "Globals.h"
#include "ShaderTableDesc.h"
#include "CurveWidget.h"
#include <vector>
#include <random>
#include <algorithm>
#include <d3d12.h>
#include <wrl/client.h>

class ComponentParticleSystem : public Component {
public:
    enum class EmitterShape {
        Point = 0,
        Box = 1,
        Sphere = 2,
        Cone = 3,
    };

    enum class BlendMode {
        Alpha = 0,
        Additive = 1,
        Premultiplied = 2,   // CPU path only; the GPU path draws it as Additive
    };

    // How a particle picks its sprite-sheet tile.
    enum class SheetMode {
        Fixed = 0,          // tile 0, or a random tile with randomFrame
        OverLifetime = 1,   // plays every tile once across the particle's life
        Fps = 2,            // plays at sheetFps from the particle's start tile; loops or holds the last tile
    };

    explicit ComponentParticleSystem(GameObject* owner);
    ~ComponentParticleSystem() override = default;

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::ParticleSystem; }

    bool enabled = true;
    bool playing = true;
    bool looping = true;
    float duration = 5.f;
    float emissionRate = 20.f;
    int maxParticles = 256;

    EmitterShape shape = EmitterShape::Cone;
    float shapeRadius = 0.5f;
    float coneAngleDeg = 25.f;

    bool worldSpace = true;

    Vector2 lifeRange = Vector2(1.f, 2.f);
    Vector2 speedRange = Vector2(1.f, 3.f);
    Vector2 sizeRange = Vector2(0.25f, 0.5f);
    Vector2 rotationRange = Vector2(-45.f, 45.f);

    Vector4 startColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 endColor = Vector4(1.f, 1.f, 1.f, 0.f);

    float startSizeMul = 1.f;
    float endSizeMul = 1.f;
    EaseCurve sizeCurve;

    Vector3 gravity = Vector3(0.f, 0.f, 0.f);

    bool useTurbulence = false;
    float turbulenceFrequency = 0.5f;
    float turbulenceStrength = 1.5f;
    int turbulenceOctaves = 3;
    float turbulenceScroll = 0.3f;

    bool useGPU = false;

    std::string texturePath;
    int sheetColumns = 1;
    int sheetRows = 1;
    bool randomFrame = false;
    SheetMode sheetMode = SheetMode::Fixed;
    float sheetFps = 12.f;
    bool sheetLoop = true;
    BlendMode blendMode = BlendMode::Alpha;
    int layer = 0;

    // CPU path: billboards stretch along their on-screen velocity, length x (1 + velocityStretch x speed).
    // 0 = round sprites. For sparks and streaks.
    float velocityStretch = 0.f;

    // Particles to spawn on the next update, whether or not the system is playing (Phoenix::VFX::Burst).
    int pendingBurst = 0;

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

    Vector4 colorAt(float t) const{
        return Vector4(startColor.x + (endColor.x - startColor.x) * t,
                       startColor.y + (endColor.y - startColor.y) * t,
                       startColor.z + (endColor.z - startColor.z) * t,
                       startColor.w + (endColor.w - startColor.w) * t);
    }
    float sizeMultiplierAt(float t) const { return startSizeMul + (endSizeMul - startSizeMul) * sizeCurve.Eval(t); }

    int frameAt(const Particle& p) const{
        const int total = (std::max)(1, sheetColumns * sheetRows);
        int f = p.frameIndex;
        if (sheetMode == SheetMode::OverLifetime){
            const float t = p.age / (std::max)(0.0001f, p.lifetime);
            f = (std::min)(total - 1, (int)(t * (float)total));
        } else if (sheetMode == SheetMode::Fps){
            const int n = p.frameIndex + (int)(p.age * (std::max)(0.f, sheetFps));
            f = sheetLoop ? n % total : (std::min)(n, total - 1);
        }
        return ((f % total) + total) % total;
    }

    int aliveCount() const{
        int n = 0;
        for (const auto& p : m_particles) if (p.alive) ++n;
        return n;
    }

    void play(){ playing = true; }
    // Starts the emission clock again (a non-looping system emits for another `duration`); live particles stay.
    void restart(){ playing = true; m_age = 0.f; m_spawnAccumulator = 0.f; }
    void stop(){ playing = false; }
    void clear(){ m_particles.clear(); m_spawnAccumulator = 0.f; m_age = 0.f; m_lastOwnerWorld = Matrix::Identity; }

private:
    void spawnParticle();
    Vector3 randomEmitDirection(std::mt19937& rng) const;
    Vector3 randomEmitPosition(std::mt19937& rng) const;

    void updateNoisePreview();

    std::vector<Particle> m_particles;
    float m_spawnAccumulator = 0.f;
    float m_age = 0.f;
    mutable std::mt19937 m_rng{ std::random_device{}() };
    Matrix m_lastOwnerWorld = Matrix::Identity; // used by worldSpace=false to track emitter movement

    Microsoft::WRL::ComPtr<ID3D12Resource> m_noisePreviewTex;
    ShaderTableDesc m_noisePreviewSRV;
    bool m_noisePreviewDirty = true;
};
