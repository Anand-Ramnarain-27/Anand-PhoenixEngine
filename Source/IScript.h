#pragma once
#include <string>

class GameObject; 

class IScript {
public:
    virtual ~IScript() = default;

    virtual void Start(GameObject* owner) {}

    virtual void Update(float dt) {}

    virtual void Destroy() {}

    virtual void Editor() {}

    virtual std::string Save() const { return "{}"; }

    virtual void Load(const std::string& json) {}

    virtual const char* getTypeName() const = 0;
};
