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
#include "SceneManager.h"
#include "SceneGraph.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <sstream>

#ifdef PHOENIX_EDITOR
#include "AshfallData.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#endif

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

namespace {

std::string toJson(const Value& v){
    StringBuffer buf; Writer<StringBuffer> w(buf); v.Accept(w);
    return buf.GetString();
}

// ---------------------------------------------------------------- one value of a field's type

bool isStringType(ScriptFieldType t){
    return t == ScriptFieldType::String || t == ScriptFieldType::Scene || t == ScriptFieldType::Prefab ||
           t == ScriptFieldType::AssetPath;
}

size_t elemSize(ScriptFieldType t){
    switch (t){
        case ScriptFieldType::Int: case ScriptFieldType::Enum: return sizeof(int);
        case ScriptFieldType::Float: return sizeof(float);
        case ScriptFieldType::Bool: return sizeof(bool);
        case ScriptFieldType::Vec3: return sizeof(float) * 3;
        case ScriptFieldType::Color: return sizeof(float) * 4;
        case ScriptFieldType::ObjectRef: return sizeof(ScriptObjectRef);
        default: return sizeof(std::string);
    }
}

int countOf(const ScriptField& f){ return f.arraySize > 0 ? f.arraySize : 1; }
void* elem(const ScriptField& f, int i){ return static_cast<char*>(f.value) + elemSize(f.type) * i; }
const void* defElem(const ScriptField& f, int i){
    return f.defaultValue ? static_cast<const char*>(f.defaultValue) + elemSize(f.type) * i : nullptr;
}

bool floatsEqual(const float* a, const float* b, int n){
    for (int i = 0; i < n; ++i) if (std::fabs(a[i] - b[i]) > 1e-6f * (std::max)(1.f, std::fabs(a[i]))) return false;
    return true;
}

bool scalarEquals(ScriptFieldType t, const void* a, const void* b){
    switch (t){
        case ScriptFieldType::Int: case ScriptFieldType::Enum: return *static_cast<const int*>(a) == *static_cast<const int*>(b);
        case ScriptFieldType::Float: return floatsEqual(static_cast<const float*>(a), static_cast<const float*>(b), 1);
        case ScriptFieldType::Bool: return *static_cast<const bool*>(a) == *static_cast<const bool*>(b);
        case ScriptFieldType::Vec3: return floatsEqual(static_cast<const float*>(a), static_cast<const float*>(b), 3);
        case ScriptFieldType::Color: return floatsEqual(static_cast<const float*>(a), static_cast<const float*>(b), 4);
        case ScriptFieldType::ObjectRef:
            return static_cast<const ScriptObjectRef*>(a)->path == static_cast<const ScriptObjectRef*>(b)->path;
        default: return *static_cast<const std::string*>(a) == *static_cast<const std::string*>(b);
    }
}

Value scalarToJson(ScriptFieldType t, const void* p, Document::AllocatorType& a){
    Value v;
    switch (t){
        case ScriptFieldType::Int: case ScriptFieldType::Enum: v.SetInt(*static_cast<const int*>(p)); break;
        case ScriptFieldType::Float: v.SetFloat(*static_cast<const float*>(p)); break;
        case ScriptFieldType::Bool: v.SetBool(*static_cast<const bool*>(p)); break;
        case ScriptFieldType::Vec3: case ScriptFieldType::Color:{
            const float* f = static_cast<const float*>(p);
            v.SetArray();
            for (int i = 0; i < (t == ScriptFieldType::Vec3 ? 3 : 4); ++i) v.PushBack(f[i], a);
            break;
        }
        case ScriptFieldType::ObjectRef:{
            const ScriptObjectRef& r = *static_cast<const ScriptObjectRef*>(p);
            v.SetObject();
            v.AddMember("path", Value(r.path.c_str(), static_cast<SizeType>(r.path.size()), a), a);
            break;
        }
        default:{
            const std::string& s = *static_cast<const std::string*>(p);
            v.SetString(s.c_str(), static_cast<SizeType>(s.size()), a);
            break;
        }
    }
    return v;
}

// Writes `v` into `p` when the JSON is compatible with the type; false (untouched) otherwise.
bool scalarFromJson(ScriptFieldType t, const Value& v, void* p){
    switch (t){
        case ScriptFieldType::Int: case ScriptFieldType::Enum:
            if (!v.IsNumber()) return false;
            *static_cast<int*>(p) = v.IsInt() ? v.GetInt() : static_cast<int>(v.GetDouble());
            return true;
        case ScriptFieldType::Float:
            if (!v.IsNumber()) return false;
            *static_cast<float*>(p) = static_cast<float>(v.GetDouble());
            return true;
        case ScriptFieldType::Bool:
            if (!v.IsBool()) return false;
            *static_cast<bool*>(p) = v.GetBool();
            return true;
        case ScriptFieldType::Vec3: case ScriptFieldType::Color:{
            const SizeType n = t == ScriptFieldType::Vec3 ? 3 : 4;
            if (!v.IsArray() || v.Size() != n) return false;
            for (SizeType i = 0; i < n; ++i) if (!v[i].IsNumber()) return false;
            float* f = static_cast<float*>(p);
            for (SizeType i = 0; i < n; ++i) f[i] = static_cast<float>(v[i].GetDouble());
            return true;
        }
        case ScriptFieldType::ObjectRef:{
            ScriptObjectRef& r = *static_cast<ScriptObjectRef*>(p);
            if (v.IsObject() && v.HasMember("path") && v["path"].IsString()) r.path = v["path"].GetString();
            else if (v.IsString()) r.path = v.GetString();
            else return false;
            r.object = nullptr;
            return true;
        }
        default:
            if (!v.IsString()) return false;
            *static_cast<std::string*>(p) = v.GetString();
            return true;
    }
}

Value fieldToJson(const ScriptField& f, Document::AllocatorType& a){
    if (f.arraySize <= 0) return scalarToJson(f.type, f.value, a);
    Value arr(kArrayType);
    for (int i = 0; i < f.arraySize; ++i) arr.PushBack(scalarToJson(f.type, elem(f, i), a), a);
    return arr;
}

void fieldFromJson(const ScriptField& f, const Value& v){
    if (f.arraySize <= 0){ scalarFromJson(f.type, v, f.value); return; }
    if (!v.IsArray()) return;
    for (int i = 0; i < f.arraySize && i < (int)v.Size(); ++i) scalarFromJson(f.type, v[i], elem(f, i));
}

bool fieldIsDefault(const ScriptField& f){
    if (!f.defaultValue) return false;
    for (int i = 0; i < countOf(f); ++i) if (!scalarEquals(f.type, elem(f, i), defElem(f, i))) return false;
    return true;
}

void resetField(const ScriptField& f){
    if (!f.defaultValue) return;
    Document tmp;
    for (int i = 0; i < countOf(f); ++i){
        // Through JSON so every type (strings, refs) copies the same way.
        Value v = scalarToJson(f.type, defElem(f, i), tmp.GetAllocator());
        scalarFromJson(f.type, v, elem(f, i));
    }
}

// Same value as the JSON `v` (or, with no JSON, as the field's default)?
bool fieldEqualsJson(const ScriptField& f, const Value* v){
    if (!v) return f.defaultValue ? fieldIsDefault(f) : true;
    Document tmp;
    Value mine = fieldToJson(f, tmp.GetAllocator());
    if (mine.IsArray() != v->IsArray()) return false;
    // Compare through decoded values (float formatting differs between writers).
    auto eq = [&](const Value& a, const Value& b) -> bool {
        if (a.IsNumber() && b.IsNumber()) return std::fabs(a.GetDouble() - b.GetDouble()) <= 1e-5 * (std::max)(1.0, std::fabs(a.GetDouble()));
        return a == b;
    };
    if (f.arraySize > 0 || f.type == ScriptFieldType::Vec3 || f.type == ScriptFieldType::Color){
        if (!v->IsArray() || mine.Size() != v->Size()) return false;
        for (SizeType i = 0; i < mine.Size(); ++i){
            const Value& a = mine[i];
            const Value& b = (*v)[i];
            if (a.IsArray()){
                if (!b.IsArray() || a.Size() != b.Size()) return false;
                for (SizeType k = 0; k < a.Size(); ++k) if (!eq(a[k], b[k])) return false;
            }
            else if (!eq(a, b)) return false;
        }
        return true;
    }
    return eq(mine, *v);
}

// The script's current field values merged over `base` (the last known "Fields" object), so a field the
// script no longer declares - or every field, while the DLL isn't loaded - keeps its saved value. A field equal
// to its default and a read-only field are left out.
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
            const bool skip = (f.flags & ScriptFieldFlag_ReadOnly) || fieldIsDefault(f);
            auto it = doc.FindMember(f.name);
            if (skip){ if (it != doc.MemberEnd()) doc.RemoveMember(it); continue; }
            Value v = fieldToJson(f, a);
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
        if (!f.name || !f.value || (f.flags & ScriptFieldFlag_ReadOnly)) continue;
        auto it = doc.FindMember(f.name);
        if (it != doc.MemberEnd()) fieldFromJson(f, it->value);
    }
}

// ---------------------------------------------------------------- hierarchy paths (ObjectRef, Keep changes)

GameObject* sceneRootOf(GameObject* go){
    GameObject* r = go;
    while (r && r->getParent()) r = r->getParent();
    return r;
}

std::string pathOf(GameObject* go){
    std::string p;
    for (GameObject* n = go; n && n->getParent(); n = n->getParent()) p = n->getName() + (p.empty() ? "" : "/" + p);
    return p;
}

GameObject* findByPath(GameObject* root, const std::string& path){
    if (!root || path.empty()) return nullptr;
    GameObject* cur = root;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')){
        GameObject* next = nullptr;
        for (GameObject* c : cur->getChildren()) if (c->getName() == part){ next = c; break; }
        if (!next) return nullptr;
        cur = next;
    }
    return cur;
}

// "Keep changes": "<path>|<class>" -> fields JSON.
std::map<std::string, std::string>& keptChanges(){
    static std::map<std::string, std::string> m;
    return m;
}

bool playing(){
    RuntimeCore* rc = app ? app->getRuntimeCore() : nullptr;
    SceneManager* sm = rc ? rc->getSceneManager() : nullptr;
    return sm && sm->getState() != SceneManager::PlayState::Stopped;
}

// ---------------------------------------------------------------- drawing

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

// "Assets/Enemies|Assets/EnemyConfigs;.json" -> folders + extension.
void parseFilter(const char* filter, std::vector<std::string>& folders, std::string& ext){
    folders.clear(); ext.clear();
    if (!filter) return;
    std::string s = filter;
    const size_t semi = s.find(';');
    if (semi != std::string::npos){ ext = s.substr(semi + 1); s = s.substr(0, semi); }
    std::stringstream ss(s);
    std::string f;
    while (std::getline(ss, f, '|')) if (!f.empty()) folders.push_back(f);
}

// Files an AssetPath field may pick: names alone ("goblin.json") when the filter lists folders - the form the
// scripts' loaders take - else project-relative paths.
std::vector<std::string> listAssets(const char* filter){
    std::vector<std::string> folders;
    std::string ext;
    parseFilter(filter, folders, ext);
    std::vector<std::string> out;
    const std::filesystem::path base = std::filesystem::path(app->getFileSystem()->GetAssetsPath()).parent_path();
    std::error_code ec;
    if (folders.empty()) folders.push_back("Assets");
    for (const std::string& folder : folders){
        const bool recursive = folder == "Assets";
        auto add = [&](const std::filesystem::path& p){
            const std::string name = p.filename().string();
            if (!ext.empty() && p.extension() != ext) return;
            if (name.find(".schema.") != std::string::npos || name == "_schema.json") return;
            const std::string v = recursive ? std::filesystem::relative(p, base, ec).generic_string() : name;
            if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        };
        if (recursive){
            for (const auto& e : std::filesystem::recursive_directory_iterator(base / folder, ec)) if (e.is_regular_file()) add(e.path());
        } else {
            for (const auto& e : std::filesystem::directory_iterator(base / folder, ec)) if (e.is_regular_file()) add(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
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

void collectPaths(GameObject* node, std::vector<std::pair<std::string, GameObject*>>& out){
    for (GameObject* c : node->getChildren()){
        out.emplace_back(pathOf(c), c);
        collectPaths(c, out);
    }
}

bool drawObjectRef(const char* label, ScriptObjectRef& ref, GameObject* owner){
    bool changed = false;
    static char filter[64] = "";
    const std::string shown = ref.path.empty() ? "(none)" : ref.path;
    if (ImGui::BeginCombo(label, shown.c_str(), ImGuiComboFlags_HeightLarge)){
        ImGui::InputTextWithHint("##filter", "filter", filter, sizeof(filter));
        if (ImGui::Selectable("(none)", ref.path.empty())){ ref.path.clear(); ref.object = nullptr; changed = true; }
        std::vector<std::pair<std::string, GameObject*>> all;
        if (GameObject* root = sceneRootOf(owner)) collectPaths(root, all);
        for (const auto& [p, go] : all){
            if (filter[0] && p.find(filter) == std::string::npos) continue;
            if (ImGui::Selectable(p.c_str(), p == ref.path)){ ref.path = p; ref.object = go; changed = true; }
        }
        ImGui::EndCombo();
    }
    // Drop a GameObject from the Hierarchy on it.
    if (ImGui::BeginDragDropTarget()){
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("GO_PTR")){
            GameObject* go = *static_cast<GameObject* const*>(pl->Data);
            if (go){ ref.path = pathOf(go); ref.object = go; changed = true; }
        }
        ImGui::EndDragDropTarget();
    }
    if (!ref.path.empty() && !findByPath(sceneRootOf(owner), ref.path)){
        ImGui::SameLine();
        ImGui::TextColored({ 1.0f, 0.6f, 0.2f, 1.0f }, "(missing)");
    }
    return changed;
}

std::vector<std::string> splitEnum(const char* names){
    std::vector<std::string> out;
    if (!names) return out;
    std::stringstream ss(names);
    std::string s;
    while (std::getline(ss, s, '|')) out.push_back(s);
    return out;
}

bool drawScalar(const ScriptField& f, const char* label, void* p, GameObject* owner){
    const bool ranged = f.rangeMin < f.rangeMax;
    switch (f.type){
        case ScriptFieldType::Int:
            return ranged ? ImGui::SliderInt(label, static_cast<int*>(p), (int)f.rangeMin, (int)f.rangeMax)
                          : ImGui::DragInt(label, static_cast<int*>(p));
        case ScriptFieldType::Float:
            return ranged ? ImGui::SliderFloat(label, static_cast<float*>(p), f.rangeMin, f.rangeMax)
                          : ImGui::DragFloat(label, static_cast<float*>(p), 0.05f);
        case ScriptFieldType::Bool: return ImGui::Checkbox(label, static_cast<bool*>(p));
        case ScriptFieldType::Vec3: return ImGui::DragFloat3(label, static_cast<float*>(p), 0.05f);
        case ScriptFieldType::Color: return ImGui::ColorEdit4(label, static_cast<float*>(p), ImGuiColorEditFlags_Float);
        case ScriptFieldType::Enum:{
            const std::vector<std::string> names = splitEnum(f.enumNames);
            int& v = *static_cast<int*>(p);
            const char* cur = v >= 0 && v < (int)names.size() ? names[v].c_str() : "?";
            bool changed = false;
            if (ImGui::BeginCombo(label, cur)){
                for (int i = 0; i < (int)names.size(); ++i)
                    if (ImGui::Selectable(names[i].c_str(), i == v)){ v = i; changed = true; }
                ImGui::EndCombo();
            }
            return changed;
        }
        case ScriptFieldType::String:{
            std::string& s = *static_cast<std::string*>(p);
            char buf[2048];
            strncpy_s(buf, s.c_str(), _TRUNCATE);
            const bool edited = f.multiline > 0
                ? ImGui::InputTextMultiline(label, buf, sizeof(buf), ImVec2(-1.f, ImGui::GetTextLineHeight() * (f.multiline + 1)))
                : ImGui::InputText(label, buf, sizeof(buf));
            if (edited){ s = buf; return true; }
            return false;
        }
        case ScriptFieldType::Scene: return drawAssetCombo(label, *static_cast<std::string*>(p), listSceneNames());
        case ScriptFieldType::Prefab: return drawAssetCombo(label, *static_cast<std::string*>(p), PrefabManager::listPrefabs());
        case ScriptFieldType::AssetPath: return drawAssetCombo(label, *static_cast<std::string*>(p), listAssets(f.assetFilter));
        case ScriptFieldType::ObjectRef: return drawObjectRef(label, *static_cast<ScriptObjectRef*>(p), owner);
    }
    return false;
}

bool drawField(const ScriptField& f, GameObject* owner){
    bool changed = false;
    if (f.arraySize > 0){
        if (ImGui::TreeNodeEx(f.name, ImGuiTreeNodeFlags_SpanAvailWidth, "%s  [%d]", f.name, f.arraySize)){
            for (int i = 0; i < f.arraySize; ++i){
                ImGui::PushID(i);
                char label[32];
                snprintf(label, sizeof(label), "[%d]", i);
                changed |= drawScalar(f, label, elem(f, i), owner);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
    else changed = drawScalar(f, f.name, f.value, owner);
    return changed;
}

// ---------------------------------------------------------------- prefab instance comparison (editor)

#ifdef PHOENIX_EDITOR
// The nearest prefab-instance root at or above `go`, and the child indices from it down to `go`.
GameObject* prefabRootOf(GameObject* go, std::vector<int>& childPath){
    childPath.clear();
    for (GameObject* cur = go; cur; cur = cur->getParent()){
        if (PrefabManager::isPrefabInstance(cur)){
            std::reverse(childPath.begin(), childPath.end());
            return cur;
        }
        GameObject* parent = cur->getParent();
        if (!parent) break;
        const auto& sib = parent->getChildren();
        childPath.push_back((int)(std::find(sib.begin(), sib.end(), cur) - sib.begin()));
    }
    childPath.clear();
    return nullptr;
}

// The prefab's saved "Fields" object for this component (one cached parse per prefab file version).
struct PrefabFieldsView {
    bool found = false;       // the prefab has this script on this object
    Document fields;          // its "Fields" ({} when it has none)
};

const PrefabFieldsView* prefabFieldsFor(GameObject* owner, const std::string& className, std::string& prefabName){
    std::vector<int> childPath;
    GameObject* root = prefabRootOf(owner, childPath);
    if (!root) return nullptr;
    prefabName = PrefabManager::getPrefabName(root);

    struct Entry { std::filesystem::file_time_type time; std::string key; PrefabFieldsView view; };
    static std::map<std::string, Entry> cache;
    std::string key = prefabName + "|" + className;
    for (int i : childPath) key += "/" + std::to_string(i);

    std::error_code ec;
    const auto time = std::filesystem::last_write_time(PrefabManager::getPrefabFilePath(prefabName), ec);
    auto it = cache.find(key);
    if (it != cache.end() && it->second.time == time) return &it->second.view;

    Entry& e = cache[key];
    e.time = time;
    e.view.found = false;
    e.view.fields.SetObject();
    Document doc;
    if (!PrefabManager::readPrefabByName(prefabName, doc) || !doc.HasMember("GameObject")) return &e.view;
    const Value* node = &doc["GameObject"];
    for (int idx : childPath){
        if (!node->HasMember("Children") || !(*node)["Children"].IsArray() || idx >= (int)(*node)["Children"].Size()) return &e.view;
        node = &(*node)["Children"][idx];
    }
    if (!node->HasMember("Components") || !(*node)["Components"].IsArray()) return &e.view;
    for (const Value& c : (*node)["Components"].GetArray()){
        if (!c.HasMember("Type") || c["Type"].GetInt() != (int)Component::Type::Script || !c.HasMember("Data")) continue;
        Document data; data.Parse(c["Data"].GetString());
        if (data.HasParseError() || !data.HasMember("ClassName") || className != data["ClassName"].GetString()) continue;
        e.view.found = true;
        if (data.HasMember("Fields") && data["Fields"].IsObject()) e.view.fields.CopyFrom(data["Fields"], e.view.fields.GetAllocator());
        break;
    }
    return &e.view;
}

// Writes one field (or its removal, when it is at its default) into the prefab file's matching component.
bool applyFieldToPrefab(GameObject* owner, const std::string& className, const ScriptField& f){
    std::vector<int> childPath;
    GameObject* root = prefabRootOf(owner, childPath);
    if (!root) return false;
    const std::string prefabName = PrefabManager::getPrefabName(root);
    Document doc;
    if (!PrefabManager::readPrefabByName(prefabName, doc) || !doc.HasMember("GameObject")) return false;
    Value* node = &doc["GameObject"];
    for (int idx : childPath){
        if (!node->HasMember("Children") || idx >= (int)(*node)["Children"].Size()) return false;
        node = &(*node)["Children"][idx];
    }
    if (!node->HasMember("Components")) return false;
    for (Value& c : (*node)["Components"].GetArray()){
        if (c["Type"].GetInt() != (int)Component::Type::Script) continue;
        Document data; data.Parse(c["Data"].GetString());
        if (data.HasParseError() || !data.HasMember("ClassName") || className != data["ClassName"].GetString()) continue;
        auto& a = data.GetAllocator();
        if (!data.HasMember("Fields") || !data["Fields"].IsObject()) data.AddMember("Fields", Value(kObjectType), a);
        Value& fields = data["Fields"];
        auto it = fields.FindMember(f.name);
        if (fieldIsDefault(f)){ if (it != fields.MemberEnd()) fields.RemoveMember(it); }
        else {
            Value v = fieldToJson(f, a);
            if (it != fields.MemberEnd()) it->value = v;
            else fields.AddMember(Value(f.name, a), v, a);
        }
        const std::string text = toJson(data);
        c["Data"].SetString(text.c_str(), static_cast<SizeType>(text.size()), doc.GetAllocator());
        return PrefabManager::writePrefabByName(prefabName, doc);
    }
    return false;
}
#endif

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
            PHX_LOG(Scene, Warning, "ScriptComponent: class '%s' not found in any loaded DLL",
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

void ComponentScript::resolveObjectRefs(){
    if (!m_script) return;
    ScriptFieldList fields;
    m_script->GetFields(fields);
    GameObject* root = sceneRootOf(owner);
    for (const ScriptField& f : fields){
        if (f.type != ScriptFieldType::ObjectRef || !f.value) continue;
        for (int i = 0; i < countOf(f); ++i){
            ScriptObjectRef& r = *static_cast<ScriptObjectRef*>(elem(f, i));
            r.object = findByPath(root, r.path);
        }
    }
}

void ComponentScript::update(float dt){
    if (!m_script) return;
    if (!m_started){
        resolveObjectRefs();   // the whole scene exists by now
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

    const bool isPlaying = playing();
    ScriptFieldList fields;
    m_script->GetFields(fields);

#ifdef PHOENIX_EDITOR
    std::string prefabName;
    const PrefabFieldsView* prefab = prefabFieldsFor(owner, m_className, prefabName);
    if (prefab && !prefab->found) prefab = nullptr;   // the prefab doesn't have this script here: no overrides
    std::vector<int> childPath;
    GameObject* prefabRoot = prefab ? prefabRootOf(owner, childPath) : nullptr;

    if (isPlaying){
        // Play edits are live and go when Play stops, unless kept.
        const std::string keepKey = pathOf(owner) + "|" + m_className;
        const bool kept = keptChanges().count(keepKey) > 0;
        if (ImGui::SmallButton(kept ? "Keep changes (kept - update)" : "Keep changes"))
            keptChanges()[keepKey] = captureFields(m_script, m_fieldsJson);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Field edits made during Play are undone when it stops.\nThis keeps this component's current values: they are put back after Stop.");
    }
#endif

    if (!fields.empty()){
        ImGui::Separator();
        bool anyChanged = false;
        for (size_t i = 0; i < fields.size(); ++i){
            const ScriptField& f = fields[i];
            if (!f.name || !f.value || (f.flags & ScriptFieldFlag_Hidden)) continue;
            const bool readOnly = (f.flags & ScriptFieldFlag_ReadOnly) != 0;
            if (readOnly && !isPlaying) continue;   // runtime state: only meaningful while playing

            if (f.space > 0.f) ImGui::Dummy(ImVec2(0.f, f.space));
            if (f.header){
                ImGui::Spacing();
                ImGui::SeparatorText(f.header);
            }
            ImGui::PushID(static_cast<int>(i));

            bool overridden = false;
#ifdef PHOENIX_EDITOR
            if (prefab && !readOnly){
                const auto it = prefab->fields.FindMember(f.name);
                overridden = !fieldEqualsJson(f, it != prefab->fields.MemberEnd() ? &it->value : nullptr);
            }
            if (overridden){
                // Unity's look: bold, with a bar in the margin.
                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x - 6.f, p.y), ImVec2(p.x - 3.f, p.y + ImGui::GetFrameHeight()),
                                                          ImGui::ColorConvertFloat4ToU32(EditorColors::Override));
                if (g_fontBold) ImGui::PushFont(g_fontBold);
            }
#endif
            if (readOnly) ImGui::BeginDisabled();
            const bool changed = drawField(f, owner);
            if (readOnly) ImGui::EndDisabled();
#ifdef PHOENIX_EDITOR
            if (overridden && g_fontBold) ImGui::PopFont();
#endif
            if (f.tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.tooltip);

            if (!readOnly && ImGui::BeginPopupContextItem("##fieldctx")){
                ImGui::TextDisabled("%s", f.name);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset to default", nullptr, false, f.defaultValue && !fieldIsDefault(f))){
                    resetField(f);
                    anyChanged = true;
                }
#ifdef PHOENIX_EDITOR
                if (prefab){
                    const std::string mark = "Script:" + m_className + "." + f.name;
                    if (ImGui::MenuItem(("Apply to prefab '" + prefabName + "'").c_str(), nullptr, false, overridden)){
                        if (applyFieldToPrefab(owner, m_className, f) && prefabRoot)
                            PrefabManager::clearPropertyOverride(prefabRoot, (int)Component::Type::Script, mark);
                    }
                    if (ImGui::MenuItem("Revert to prefab", nullptr, false, overridden)){
                        const auto it = prefab->fields.FindMember(f.name);
                        if (it != prefab->fields.MemberEnd()) fieldFromJson(f, it->value);
                        else resetField(f);
                        if (prefabRoot) PrefabManager::clearPropertyOverride(prefabRoot, (int)Component::Type::Script, mark);
                        anyChanged = true;
                    }
                }
#endif
                ImGui::EndPopup();
            }
            ImGui::PopID();

            if (changed){
                anyChanged = true;
#ifdef PHOENIX_EDITOR
                if (prefabRoot)
                    PrefabManager::markPropertyOverride(prefabRoot, (int)Component::Type::Script, "Script:" + m_className + "." + f.name);
#endif
            }
        }
        if (anyChanged) m_fieldsJson = captureFields(m_script, m_fieldsJson);
    }

#ifdef PHOENIX_EDITOR
    drawConfigViews(fields);
#endif

    ImGui::Separator();
    m_script->Editor();
}

void ComponentScript::drawConfigViews(const ScriptFieldList& fields){
#ifdef PHOENIX_EDITOR
    // A .json AssetPath field (an enemy's ConfigPath): the file's contents right here, editable. With a
    // ConfigOverrides field linked to it, edits are this instance's overrides (bold, with reset) instead.
    for (const ScriptField& f : fields){
        if (f.type != ScriptFieldType::AssetPath || f.arraySize > 0 || !f.value) continue;
        const std::string& value = *static_cast<const std::string*>(f.value);
        const std::string asset = AshfallData::ResolveAsset(value, f.assetFilter);
        if (asset.size() < 5 || asset.compare(asset.size() - 5, 5, ".json") != 0) continue;

        std::string* overrides = nullptr;
        for (const ScriptField& o : fields)
            if ((o.flags & ScriptFieldFlag_ConfigOverrides) && o.link && f.name && strcmp(o.link, f.name) == 0 &&
                o.type == ScriptFieldType::String && o.value)
                overrides = static_cast<std::string*>(o.value);

        ImGui::PushID(f.name);
        const std::string title = std::string("Config: ") + asset + "###cfg";
        if (ImGui::CollapsingHeader(title.c_str())){
            AshfallData::Doc* doc = AshfallData::Open(asset);
            if (!doc){ ImGui::TextDisabled("can't read %s", asset.c_str()); ImGui::PopID(); continue; }
            if (overrides){
                ImGui::TextDisabled("Edits here override this object only (bold). The file: Open in Ashfall Data.");
                if (ImGui::SmallButton("Open in Ashfall Data")) AshfallData::RequestFocus(asset);
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset all overrides") && !overrides->empty()){
                    overrides->clear();
                    m_fieldsJson = captureFields(m_script, m_fieldsJson);
                }
                if (AshfallData::DrawTree(*doc, overrides)) m_fieldsJson = captureFields(m_script, m_fieldsJson);
            } else {
                AshfallData::DrawToolbar(*doc, true);
                AshfallData::DrawTree(*doc, nullptr);
            }
        }
        ImGui::PopID();
    }
#else
    (void)fields;
#endif
}

int ComponentScript::applyKeptChanges(SceneGraph* scene){
    auto& kept = keptChanges();
    if (kept.empty() || !scene) return 0;
    int applied = 0;
    GameObject* root = scene->getRoot();
    for (const auto& [key, json] : kept){
        const size_t bar = key.rfind('|');
        if (bar == std::string::npos) continue;
        GameObject* go = findByPath(root, key.substr(0, bar));
        const std::string cls = key.substr(bar + 1);
        if (!go) continue;
        for (const auto& c : go->getComponents()){
            if (c->getType() != Component::Type::Script) continue;
            auto* cs = static_cast<ComponentScript*>(c.get());
            if (cs->m_className != cls || !cs->m_script) continue;
            applyFields(cs->m_script, json);
            cs->m_fieldsJson = captureFields(cs->m_script, cs->m_fieldsJson);
            ++applied;
            break;
        }
    }
    kept.clear();
    return applied;
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
