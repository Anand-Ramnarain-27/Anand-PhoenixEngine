#pragma once
#include "Component.h"
#include "ParticleSystem.h"

// Scene component that configures a GPU particle emitter.
// Place this on any GameObject — the transform position is used as the spawn point.
class ComponentParticleEmitter : public Component {
public:
    explicit ComponentParticleEmitter(GameObject* owner);
    ~ComponentParticleEmitter() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::ParticleEmitter; }

    // Fills an EmitterDesc from this component's settings and the owner's world transform.
    EmitterDesc buildDesc() const;

    Vector3 velocity    = Vector3(0.f, 4.f, 0.f);
    Vector3 velocityVar = Vector3(1.f, 1.f, 1.f);
    Vector3 gravity     = Vector3(0.f, -4.f, 0.f);
    Vector4 colourStart = Vector4(1.f, 0.6f, 0.1f, 1.f);
    Vector4 colourEnd   = Vector4(0.2f, 0.1f, 0.8f, 0.f);
    float   lifetime    = 2.5f;
    float   lifetimeVar = 1.0f;
    float   halfSize    = 0.08f;
    int     spawnPerSec = 120;
    bool    enabled     = true;
};
