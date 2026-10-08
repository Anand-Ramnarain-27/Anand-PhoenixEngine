#pragma once
// Base class of everything that can be attached to a GameObject.

#include <string>

struct ID3D12GraphicsCommandList;
class GameObject;

/// A GameObject's behaviour or data. Components save and load themselves as JSON (onSave / onLoad) and draw their
/// own Inspector UI (onEditor). The layout of this class is mirrored in GameScript's EngineDecls: don't reorder
/// or add members without re-exporting.
class Component {
public:
    /// Saved as an integer in scenes and prefabs, and used by GameScript (Phoenix::VFX): existing values never
    /// change, new types go at the end.
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
        CheckBox = 23,
        Slider = 24,
        InputBox = 25,
        RadioGroup = 26,
    };

    explicit Component(GameObject* owner) : owner(owner){}
    virtual ~Component() = default;

    virtual void render(ID3D12GraphicsCommandList*){}
    virtual void update(float){}
    virtual void onEditor(){}
    virtual void onDrawGizmos(){}
    /// Writes this component's fields as a JSON object.
    virtual void onSave(std::string& outJson) const {}
    /// Reads what onSave wrote; missing keys keep their defaults, so old files still load.
    virtual void onLoad(const std::string& json){}
    virtual Type getType() const = 0;

protected:
    GameObject* owner = nullptr;
};
