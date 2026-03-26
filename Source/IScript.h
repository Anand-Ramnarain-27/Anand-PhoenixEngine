#pragma once
#include <string>

class GameObject;

class IScript {
public:
    virtual ~IScript() = default;

    virtual void onStart(GameObject* owner) {}

    virtual void onUpdate(float dt) {}

    virtual void onDestroy() {}

    virtual void onEditor() {}

    virtual std::string onSave() const { return "{}"; }

    virtual void onLoad(const std::string& json) {}

    virtual const char* getTypeName() const = 0;
};
