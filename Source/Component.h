#pragma once
#include <string>

struct ID3D12GraphicsCommandList;
class GameObject;

class Component {
public:
    enum class Type {
        Transform = 0,
        Mesh = 1,
        Camera = 2,
        DirectionalLight = 3,
        PointLight = 4,
        SpotLight = 5,
        Script = 6,
        Animation = 7,
        CharacterMotion = 8,
        SimpleCharacterController = 9,
        Rigidbody = 10,
        Bounds = 11,
        Decal = 12,
        Billboard = 13,
        ParticleSystem = 14,
        Trail = 15,
        AIAgent = 16,
        Transform2D = 17,
        Canvas = 18,
        Image = 19,
        Label = 20,
        Button = 21,
        ProgressBar = 22,
    };

    explicit Component(GameObject* owner) : owner(owner){}
    virtual ~Component() = default;

    virtual void render(ID3D12GraphicsCommandList*){}
    virtual void update(float){}
    virtual void onEditor(){}
    virtual void onDrawGizmos(){}
    virtual void onSave(std::string& outJson) const {}
    virtual void onLoad(const std::string& json){}
    virtual Type getType() const = 0;

protected:
    GameObject* owner = nullptr;
};
