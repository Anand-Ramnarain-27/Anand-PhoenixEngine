#pragma once
#include "IScript.h"
#include "ScriptExport.h"

class SCRIPT_API EnemyScript : public IScript {
public:
    EnemyScript();
    void        onStart(GameObject* owner)         override;
    void        onUpdate(float dt)                   override;
    void        onDestroy()                           override;
    void        onEditor()                           override;
    std::string onSave() const                     override;
    void        onLoad(const std::string& json)    override;
    const char* getTypeName() const override { return "EnemyScript"; }

private:
    GameObject* m_owner = nullptr;
    float       m_detectionRange = 10.0f;
    bool        m_isAggro = false;
};

extern "C" SCRIPT_API IScript* Create_EnemyScript();
