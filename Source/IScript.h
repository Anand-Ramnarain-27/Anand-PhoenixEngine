#pragma once
#include <string>

class GameObject;

// Every script must inherit from this interface.
// The engine calls these lifecycle methods.
class IScript {
public:
    virtual ~IScript() = default;

    // Called once when the component is first activated
    virtual void onStart(GameObject* owner) {}

    // Called every frame while the scene is playing
    virtual void onUpdate(float dt) {}

    // Called when the component is destroyed
    virtual void onDestroy() {}

    // Editor: draw any ImGui controls for exposed variables
    virtual void onEditor() {}

    // Serialise the script's exposed variables to JSON
    virtual std::string onSave() const { return "{}"; }

    // Deserialise previously saved variable state
    virtual void onLoad(const std::string& json) {}

    // Human-readable class name used in registry lookup
    virtual const char* getTypeName() const = 0;
};
