#include "Globals.h"
#include "ScriptComponent.h"
#include "HotReloadManager.h"
#include "GameObject.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <imgui.h>

using namespace rapidjson;

ScriptComponent::ScriptComponent(GameObject* owner) : Component(owner) {}

ScriptComponent::~ScriptComponent() {
    if (m_script) { m_script->onDestroy(); delete m_script; }
}

void ScriptComponent::setScriptClass(const std::string& className,
    HotReloadManager* mgr) {
    if (m_script) { m_script->onDestroy(); delete m_script; m_script = nullptr; }
    m_className = className;
    m_started = false;
    if (mgr) {
        m_script = mgr->createScript(className);
        if (!m_script)
            LOG("ScriptComponent: class '%s' not found in any loaded DLL",
                className.c_str());
    }
}

void ScriptComponent::onDllReloaded(HotReloadManager* mgr) {
    std::string savedState;
    if (m_script) {
        savedState = m_script->onSave();
        m_script->onDestroy();
        delete m_script;
        m_script = nullptr;
        m_started = false;
    }
    if (!m_className.empty() && mgr) {
        m_script = mgr->createScript(m_className);
        if (m_script && !savedState.empty())
            m_script->onLoad(savedState);
    }
}

void ScriptComponent::update(float dt) {
    if (!m_script) return;
    if (!m_started) {
        m_script->onStart(owner);
        m_started = true;
    }
    m_script->onUpdate(dt);
}

void ScriptComponent::onEditor() {
    ImGui::Text("Script: %s",
        m_className.empty() ? "<none>" : m_className.c_str());
    if (!m_script) {
        ImGui::TextColored({ 1,0.4f,0.4f,1 }, "Not loaded");
        return;
    }
    ImGui::Separator();
    m_script->onEditor();
}

void ScriptComponent::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value cn; cn.SetString(m_className.c_str(), a);
    doc.AddMember("ClassName", cn, a);
    if (m_script) {
        Value sd; sd.SetString(m_script->onSave().c_str(), a);
        doc.AddMember("ScriptData", sd, a);
    }
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ScriptComponent::onLoad(const std::string& jsonStr) {
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("ClassName"))
        m_className = doc["ClassName"].GetString();
    if (m_script && doc.HasMember("ScriptData"))
        m_script->onLoad(doc["ScriptData"].GetString());
}
