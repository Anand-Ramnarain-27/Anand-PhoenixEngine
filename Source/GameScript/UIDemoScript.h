#pragma once
#include "IScript.h"
#include "ScriptExport.h"
#include "API/Phoenix_UI.h"

// Example of the UI script API. Attach to a Button: clicking it counts up and writes the count into the first
// child Label. Doubles as a link test for the Phoenix::UI surface.
class SCRIPT_API UIDemoScript : public IScript {
public:
    UIDemoScript();

    void Start(GameObject* owner) override;
    void Update(float dt) override;
    void Destroy() override;
    void Editor() override;
    std::string Save() const override;
    void Load(const std::string& json) override;
    const char* getTypeName() const override { return "UIDemoScript"; }

private:
    void refreshLabel();

    GameObject* m_owner = nullptr;
    GameObject* m_label = nullptr;
    GameObject* m_bar = nullptr;
    GameObject* m_second = nullptr;
    GameObject* m_health = nullptr;
    GameObject* m_healthText = nullptr;
    int m_clicks = 0;
    Phoenix::UIListener m_onClick;
    Phoenix::UIListener m_onEnter;
    Phoenix::UIListener m_onExit;
    Phoenix::UIListener m_onToggle;
    Phoenix::UIListener m_onSlide;
};

extern "C" SCRIPT_API IScript* Create_UIDemoScript();
