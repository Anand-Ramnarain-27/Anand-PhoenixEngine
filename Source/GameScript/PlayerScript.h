#pragma once
#include "IScript.h"
#include "ScriptExport.h"

class SCRIPT_API PlayerScript : public IScript {
public:
    PlayerScript();

    void Start(GameObject* owner) override;
    void Update(float dt) override;
    void Destroy() override;
    void Editor() override;
    std::string Save() const override;
    void Load(const std::string& json) override;
    const char* getTypeName() const override { return "PlayerScript"; }

private:
    GameObject* m_owner = nullptr;
    float m_speed = 5.0f;
    float m_timer = 0.0f;
};

extern "C" SCRIPT_API IScript* Create_PlayerScript();
