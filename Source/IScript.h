#pragma once
#include <string>

class GameObject;

class IScript {
public:
    virtual ~IScript() = default;

    virtual void Start(GameObject* owner) {}

    virtual void Update(float dt) {}

    // Gap 3 (AI culling): called once per frame before Update(). The engine
    // (ComponentScript) computes whether the owning mesh is visible in the
    // game view and its distance to the active game camera, plus the
    // configured AI cull distance/tick rate (ModuleCamera::aiCullDistance /
    // aiCullTickRate), and passes them as plain data — GameScript DLLs cannot
    // link directly against Engine internals like GameObject/ComponentMesh.
    // Return false to skip this frame's Update() call.
    virtual bool shouldTickAI(bool isVisible, float distanceToCamera,
        float aiCullDistance, int aiCullTickRate){
        return true;
    }

    virtual void Destroy() {}

    virtual void Editor() {}

    virtual std::string Save() const { return "{}"; }

    virtual void Load(const std::string& json) {}

    virtual const char* getTypeName() const = 0;
};
