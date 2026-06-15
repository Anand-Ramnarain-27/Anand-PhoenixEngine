#pragma once
#include <string>

class GameObject;

class IScript {
public:
    virtual ~IScript() = default;

    virtual void Start(GameObject* owner){}

    virtual void Update(float dt){}

    virtual bool shouldTickAI(bool isVisible, float distanceToCamera,
        float aiCullDistance, int aiCullTickRate){
        return true;
    }

    virtual void Destroy(){}

    virtual void Editor(){}

    virtual std::string Save() const { return "{}"; }

    virtual void Load(const std::string& json){}

    virtual const char* getTypeName() const = 0;
};
