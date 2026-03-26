#pragma once
#include "Component.h"
#include "IScript.h"
#include <string>
#include <memory>

class HotReloadManager;

class ScriptComponent : public Component {
public:
    explicit ScriptComponent(GameObject* owner);
    ~ScriptComponent() override;

    void setScriptClass(const std::string& className,
        HotReloadManager* mgr);

    void onDllReloaded(HotReloadManager* mgr);

    // Component overrides
    void update(float dt)      override;
    void onEditor()            override;
    void onSave(std::string& out) const override;
    void onLoad(const std::string& json) override;
    Type getType() const       override { return Type::Script; }

    const std::string& getClassName() const { return m_className; }
    bool hasInstance() const { return m_script != nullptr; }

private:
    std::string m_className;
    IScript* m_script = nullptr;  
    bool        m_started = false;
};
