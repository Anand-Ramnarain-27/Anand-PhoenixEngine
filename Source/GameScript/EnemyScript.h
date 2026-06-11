#pragma once
#include "IScript.h"
#include "ScriptExport.h"

class SCRIPT_API EnemyScript : public IScript {
public:
    EnemyScript();
    void Start(GameObject* owner) override;
    void Update(float dt) override;

    // Gap 3 (AI culling): when off-screen and farther than aiCullDistance
    // from the active game camera, only tick every aiCullTickRate-th frame
    // instead of every frame. Engine-side data is passed in as plain values.
    bool shouldTickAI(bool isVisible, float distanceToCamera,
        float aiCullDistance, int aiCullTickRate) override;

    void Destroy() override;
    void Editor() override;
    std::string Save() const override;
    void Load(const std::string& json) override;
    const char* getTypeName() const override { return "EnemyScript"; }

private:
    GameObject* m_owner = nullptr;
    float m_detectionRange = 10.0f;
    bool m_isAggro = false;

    // Gap 3 (AI culling): counts frames since this script started ticking,
    // used to throttle Update() to 1/aiCullTickRate when off-screen and far away.
    int m_aiFrameCounter = 0;
};

extern "C" SCRIPT_API IScript* Create_EnemyScript();
