#pragma once
#include "IScript.h"
#include "ScriptExport.h"

class SCRIPT_API EnemyScript : public IScript {
public:
    EnemyScript();
    void        Start(GameObject* owner)         override;
    void        Update(float dt)                   override;
    void        Destroy()                           override;
    void        Editor()                           override;
    std::string Save() const                     override;
    void        Load(const std::string& json)    override;
    const char* getTypeName() const override { return "EnemyScript"; }

private:
    GameObject* m_owner = nullptr;
    float       m_detectionRange = 10.0f;
    bool        m_isAggro = false;
};

extern "C" SCRIPT_API IScript* Create_EnemyScript();
