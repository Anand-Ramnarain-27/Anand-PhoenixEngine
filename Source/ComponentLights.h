#pragma once
#include "Component.h"
#include "ModuleD3D12.h"

class ComponentDirectionalLight : public Component {
public:
    explicit ComponentDirectionalLight(GameObject* owner);
    ~ComponentDirectionalLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::DirectionalLight; }

    Vector3 direction = Vector3(-0.5f, -1.0f, -0.5f);
    Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    bool enabled = true;
};

class ComponentPointLight : public Component {
public:
    explicit ComponentPointLight(GameObject* owner);
    ~ComponentPointLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::PointLight; }

    Vector3 color = Vector3(1.f, 1.f, 1.f);
    float intensity = 1.0f;
    float radius = 5.0f;
    bool enabled = true;
};

class ComponentSpotLight : public Component {
public:
    explicit ComponentSpotLight(GameObject* owner);
    ~ComponentSpotLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::SpotLight; }

    Vector3 direction = Vector3(0.f, -1.f, 0.f);
    Vector3 color = Vector3(1.f, 1.f, 1.f);
    float intensity = 1.0f;
    float innerAngle = 15.0f;
    float outerAngle = 30.0f;
    float radius = 10.0f;
    bool enabled = true;
};