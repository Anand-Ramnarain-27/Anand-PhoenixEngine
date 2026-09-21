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
    m_onClick = m_onEnter = m_onExit = {};
}

void UIDemoScript::refreshLabel(){
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
