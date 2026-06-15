#pragma once
#include "Component.h"
#include "Globals.h"
#include "ShaderTableDesc.h"
#include "CurveWidget.h"
#include <vector>
#include <random>
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
    BlendMode blendMode = BlendMode::Alpha;
    int layer = 0;

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

    void play(){ playing = true; }
    void stop(){ playing = false; }
    void clear(){ m_particles.clear(); m_spawnAccumulator = 0.f; m_age = 0.f; }

private:
    void spawnParticle();
    Vector3 randomEmitDirection(std::mt19937& rng) const;
    Vector3 randomEmitPosition(std::mt19937& rng) const;

    void updateNoisePreview();

    std::vector<Particle> m_particles;
    float m_spawnAccumulator = 0.f;
    float m_age = 0.f;
    mutable std::mt19937 m_rng{ std::random_device{}() };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_noisePreviewTex;
    ShaderTableDesc m_noisePreviewSRV;
    bool m_noisePreviewDirty = true;
};
