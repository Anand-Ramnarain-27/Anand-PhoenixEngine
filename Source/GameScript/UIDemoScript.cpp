#include "UIDemoScript.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

#include "Globals.h"
#include "API/PhoenixAPI.h"
#include "ComponentLabel.h"

UIDemoScript::UIDemoScript() = default;

void UIDemoScript::Start(GameObject* owner){
    m_owner = owner;
    m_label = nullptr;
    m_bar = Scene::Find("Click Progress");
    for (GameObject* child : owner->getChildren()){
        if (child->getComponent<ComponentLabel>()){ m_label = child; break; }
    }

    // Callbacks capture `this`, so they are removed in Destroy().
    m_onClick = UI::OnClick(owner, [this](){
        ++m_clicks;
        refreshLabel();
    });
    m_onEnter = UI::OnHoverEnter(owner, [](){ Debug::Log("UIDemoScript: hover enter"); });
    m_onExit = UI::OnHoverExit(owner, [](){ Debug::Log("UIDemoScript: hover exit"); });

    // The checkbox enables/disables the second button; the slider drives the health bar and its text.
    m_second = Scene::Find("Button Second");
    m_health = Scene::Find("Health Bar");
    m_healthText = Scene::Find("Health Text");
    m_onToggle = UI::OnToggled(Scene::Find("Test Checkbox"), [this](bool checked){
        UI::SetInteractable(m_second, checked);
    });
    m_onSlide = UI::OnValueChanged(Scene::Find("Test Slider"), [this](float value){
        UI::SetProgress(m_health, value / 100.f);
        UI::SetText(m_healthText, "HP " + std::to_string((int)value) + " / 100");
    });

    refreshLabel();
}

void UIDemoScript::Update(float dt){
    // Polling works too: it reports the frame after the event, with no callback to manage.
    if (UI::WasClicked(m_owner) && UI::IsPointerOverUI())
        Debug::Log("UIDemoScript: clicked (polled)");
}

void UIDemoScript::Destroy(){
    UI::RemoveListener(m_onClick);
    UI::RemoveListener(m_onEnter);
    UI::RemoveListener(m_onExit);
    UI::RemoveListener(m_onToggle);
    UI::RemoveListener(m_onSlide);
    m_onClick = m_onEnter = m_onExit = m_onToggle = m_onSlide = {};
}

void UIDemoScript::refreshLabel(){
    if (m_bar) UI::SetProgress(m_bar, float(m_clicks % 11) / 10.f);   // 0..1, wraps after ten clicks
    if (!m_label) return;
    UI::SetText(m_label, "Clicked " + std::to_string(m_clicks));
}

void UIDemoScript::Editor(){
}

std::string UIDemoScript::Save() const{
    Document doc; doc.SetObject();
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    return buf.GetString();
}

void UIDemoScript::Load(const std::string& json){
    Document doc; doc.Parse(json.c_str());
}

IScript* Create_UIDemoScript(){ return new UIDemoScript(); }
