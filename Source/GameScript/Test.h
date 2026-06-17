#pragma once
#include "IScript.h"
#include "ScriptExport.h"

class SCRIPT_API Test : public IScript {
public:
    Test();

    void Start(GameObject* owner) override;
    void Update(float dt) override;
    void Destroy() override;
    void Editor() override;
    std::string Save() const override;
    void Load(const std::string& json) override;
    const char* getTypeName() const override { return "Test"; }

private:
    GameObject* m_owner = nullptr;
};

extern "C" SCRIPT_API IScript* Create_Test();
