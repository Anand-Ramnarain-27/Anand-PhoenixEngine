#include "Globals.h"
#include "ComponentScript.h"
#include "HotReloadManager.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "ModuleCamera.h"
#include "ModuleFileSystem.h"
#include "PrefabManager.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

namespace {

std::string toJson(const Value& v){
    StringBuffer buf; Writer<StringBuffer> w(buf); v.Accept(w);
    return buf.GetString();
}

// The script's current field values merged over `base` (the last known "Fields" object), so a field the
// script no longer declares - or every field, while the DLL isn't loaded - keeps its saved value.
std::string captureFields(IScript* script, const std::string& base){
    Document doc;
    if (!base.empty()) doc.Parse(base.c_str());
    if (doc.HasParseError() || !doc.IsObject()) doc.SetObject();
    auto& a = doc.GetAllocator();

    if (script){
        ScriptFieldList fields;
        script->GetFields(fields);
        for (const ScriptField& f : fields){
            if (!f.name || !f.value) continue;
            Value v;
            switch (f.type){
                case ScriptFieldType::Int:   v.SetInt(*static_cast<const int*>(f.value)); break;
                case ScriptFieldType::Float: v.SetFloat(*static_cast<const float*>(f.value)); break;
                case ScriptFieldType::Bool:  v.SetBool(*static_cast<const bool*>(f.value)); break;
                case ScriptFieldType::String:
                case ScriptFieldType::Scene:
                case ScriptFieldType::Prefab:{
                    const std::string& s = *static_cast<const std::string*>(f.value);
                    v.SetString(s.c_str(), static_cast<SizeType>(s.size()), a);
                    break;
                }
                case ScriptFieldType::Vec3:{
                    const float* p = static_cast<const float*>(f.value);
                    v.SetArray(); v.PushBack(p[0], a).PushBack(p[1], a).PushBack(p[2], a);
                    break;
                }
            }
            auto it = doc.FindMember(f.name);
            if (it != doc.MemberEnd()) it->value = v;
            else doc.AddMember(Value(f.name, a), v, a);
        }
    }
    return toJson(doc);
}

// Writes every saved value in `fieldsJson` that the script still declares (same name, compatible type).
void applyFields(IScript* script, const std::string& fieldsJson){
    if (!script || fieldsJson.empty()) return;
    Document doc; doc.Parse(fieldsJson.c_str());
    if (doc.HasParseError() || !doc.IsObject()) return;

    ScriptFieldList fields;
    script->GetFields(fields);
    for (const ScriptField& f : fields){
        if (!f.name || !f.value) continue;
        auto it = doc.FindMember(f.name);
        if (it == doc.MemberEnd()) continue;
        const Value& v = it->value;
        switch (f.type){
            case ScriptFieldType::Int:
                if (v.IsNumber()) *static_cast<int*>(f.value) = v.IsInt() ? v.GetInt() : static_cast<int>(v.GetDouble());
                break;
            case ScriptFieldType::Float:
                if (v.IsNumber()) *static_cast<float*>(f.value) = static_cast<float>(v.GetDouble());
                break;
            case ScriptFieldType::Bool:
                if (v.IsBool()) *static_cast<bool*>(f.value) = v.GetBool();
                break;
            case ScriptFieldType::String:
            case ScriptFieldType::Scene:
            case ScriptFieldType::Prefab:
                if (v.IsString()) *static_cast<std::string*>(f.value) = v.GetString();
                break;
            case ScriptFieldType::Vec3:
                if (v.IsArray() && v.Size() == 3 && v[0].IsNumber() && v[1].IsNumber() && v[2].IsNumber()){
                    float* p = static_cast<float*>(f.value);
                    for (SizeType i = 0; i < 3; ++i) p[i] = static_cast<float>(v[i].GetDouble());
                }
                break;
        }
    }
}

std::vector<std::string> listSceneNames(){
    std::vector<std::string> names;
    const std::string dir = app->getFileSystem()->GetLibraryPath() + "Scenes";
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)){
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        std::string stem = e.path().stem().string();
        if (stem != "temp_scene") names.push_back(stem);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Combo over `options`, plus "(none)". Flags a value that no longer matches any option.
bool drawAssetCombo(const char* label, std::string& value, const std::vector<std::string>& options){
    bool changed = false;
    if (ImGui::BeginCombo(label, value.empty() ? "(none)" : value.c_str())){
        if (ImGui::Selectable("(none)", value.empty())){ value.clear(); changed = true; }
        for (const std::string& o : options)
            if (ImGui::Selectable(o.c_str(), o == value)){ value = o; changed = true; }
        ImGui::EndCombo();
    }
    if (!value.empty() && std::find(options.begin(), options.end(), value) == options.end()){
        ImGui::SameLine();
        ImGui::TextColored({ 1.0f, 0.6f, 0.2f, 1.0f }, "(missing)");
    }
    return changed;
}

bool drawField(const ScriptField& f){
    bool changed = false;
    switch (f.type){
        case ScriptFieldType::Int:   changed = ImGui::DragInt(f.name, static_cast<int*>(f.value)); break;
        case ScriptFieldType::Float: changed = ImGui::DragFloat(f.name, static_cast<float*>(f.value), 0.05f); break;
        case ScriptFieldType::Bool:  changed = ImGui::Checkbox(f.name, static_cast<bool*>(f.value)); break;
        case ScriptFieldType::Vec3:  changed = ImGui::DragFloat3(f.name, static_cast<float*>(f.value), 0.05f); break;
        case ScriptFieldType::String:{
            std::string& s = *static_cast<std::string*>(f.value);
            char buf[512];
            strncpy_s(buf, s.c_str(), _TRUNCATE);
            if (ImGui::InputText(f.name, buf, sizeof(buf))){ s = buf; changed = true; }
            break;
        }
        case ScriptFieldType::Scene:
            changed = drawAssetCombo(f.name, *static_cast<std::string*>(f.value), listSceneNames());
            break;
        case ScriptFieldType::Prefab:
            changed = drawAssetCombo(f.name, *static_cast<std::string*>(f.value), PrefabManager::listPrefabs());
            break;
    }
    if (f.tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.tooltip);
    return changed;
}

} // namespace

ComponentScript::ComponentScript(GameObject* owner) : Component(owner){}

ComponentScript::~ComponentScript(){
    if (m_script){
        m_script->Destroy();
        delete m_script;
    }
}

void ComponentScript::setScriptClass(const std::string& className,
    HotReloadManager* mgr){
    if (m_script){
        m_script->Destroy();
        delete m_script;
        m_script = nullptr;
    }
    if (className != m_className) m_fieldsJson.clear();
    m_className = className;
    m_started = false;
    if (mgr){
        m_script = mgr->createScript(className);
        if (!m_script)
            LOG("ScriptComponent: class '%s' not found in any loaded DLL",
                className.c_str());
    }
}

void ComponentScript::onDllReloaded(HotReloadManager* mgr){
    std::string savedState;
    if (m_script){
        savedState = m_script->Save();
        m_fieldsJson = captureFields(m_script, m_fieldsJson);
        m_script->Destroy();
        delete m_script;
        m_script = nullptr;
        m_started = false;
    }
    if (!m_className.empty() && mgr){
        m_script = mgr->createScript(m_className);
        if (m_script && !savedState.empty())
            m_script->Load(savedState);
        applyFields(m_script, m_fieldsJson);
    }
}

void ComponentScript::update(float dt){
    if (!m_script) return;
    if (!m_started){
        m_script->Start(owner);
        m_started = true;
    }

    bool isVisible = true;
    if (auto* mesh = owner->getComponent<ComponentMesh>())
        isVisible = mesh->isVisible();

    float distanceToCamera = 0.0f;
    float aiCullDistance = 50.0f;
    int aiCullTickRate = 10;
    if (ModuleCamera* camera = app->getCamera()){
        aiCullDistance = camera->aiCullDistance;
        aiCullTickRate = camera->aiCullTickRate;
        if (GameObject* activeCam = camera->getActiveCamera()){
            if (auto* camTransform = activeCam->getTransform()){
                if (auto* myTransform = owner->getTransform()){
                    distanceToCamera = Vector3::Distance(
                        myTransform->getGlobalMatrix().Translation(),
                        camTransform->getGlobalMatrix().Translation());
                }
            }
        }
    }

    if (!m_script->shouldTickAI(isVisible, distanceToCamera, aiCullDistance, aiCullTickRate)) return;
    m_script->Update(dt);
}

void ComponentScript::onEditor(){
    ImGui::Text("Script class: %s",
        m_className.empty() ? "<none assigned>" : m_className.c_str());
    if (!m_script){
        ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "DLL not loaded");
        return;
    }

    ScriptFieldList fields;
    m_script->GetFields(fields);
    if (!fields.empty()){
        ImGui::Separator();
        for (size_t i = 0; i < fields.size(); ++i){
            if (!fields[i].name || !fields[i].value) continue;
            ImGui::PushID(static_cast<int>(i));
            drawField(fields[i]);
            ImGui::PopID();
        }
    }

    ImGui::Separator();
    m_script->Editor();
}

void ComponentScript::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value cn; cn.SetString(m_className.c_str(), a);
    doc.AddMember("ClassName", cn, a);
    if (m_script){
        Value sd; sd.SetString(m_script->Save().c_str(), a);
        doc.AddMember("ScriptData", sd, a);
    }
    if (m_script || !m_fieldsJson.empty()){
        Document fields; fields.Parse(captureFields(m_script, m_fieldsJson).c_str());
        if (!fields.HasParseError() && fields.IsObject() && fields.MemberCount() > 0){
            Value fv; fv.CopyFrom(fields, a);
            doc.AddMember("Fields", fv, a);
        }
    }
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentScript::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("ClassName")){
        // setScriptClass() (not a plain m_className assignment) is required here -
        // it's what actually reconnects m_script to a live instance via
        // HotReloadManager::createScript(). Without it, a component restored from
        // a saved scene/prefab keeps its class name but never gets a script
        // instance, and every caller (scene load, prefab instantiate, Play/Stop's
        // temp-scene restore) would need to remember to reconnect it themselves.
        RuntimeCore* rc = app ? app->getRuntimeCore() : nullptr;
        setScriptClass(doc["ClassName"].GetString(), rc ? rc->getHotReloadManager() : nullptr);
    }
    m_fieldsJson.clear();
    if (doc.HasMember("Fields") && doc["Fields"].IsObject())
        m_fieldsJson = toJson(doc["Fields"]);
    if (m_script && doc.HasMember("ScriptData"))
        m_script->Load(doc["ScriptData"].GetString());
    // Fields go on after Load(), so they win over anything a script also writes into its own ScriptData.
    applyFields(m_script, m_fieldsJson);
}
