#include "Globals.h"
#include "AshfallTrapsBuilder.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "SceneGraph.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentFactory.h"
#include "ComponentMesh.h"
#include "ComponentScript.h"
#include "HotReloadManager.h"
#include "PrefabManager.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace {

using rapidjson::Value;

std::vector<std::string> splitPath(const std::string& path){
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string p;
    while (std::getline(ss, p, '/')) if (!p.empty()) parts.push_back(p);
    return parts;
}

GameObject* childNamed(GameObject* parent, const std::string& name, int index = 0){
    for (GameObject* c : parent->getChildren())
        if (c->getName() == name && index-- == 0) return c;
    return nullptr;
}

// "Root/Group/Child": the first segment is a top-level object's name.
GameObject* findPath(SceneGraph* scene, const std::string& path){
    const std::vector<std::string> parts = splitPath(path);
    if (parts.empty()) return nullptr;
    GameObject* node = scene->findGameObjectByName(parts[0]);
    for (size_t i = 1; node && i < parts.size(); ++i) node = childNamed(node, parts[i]);
    return node;
}

GameObject* ensurePath(SceneGraph* scene, const std::string& path, int& created){
    const std::vector<std::string> parts = splitPath(path);
    if (parts.empty()) return nullptr;
    GameObject* node = scene->findGameObjectByName(parts[0]);
    if (!node) return nullptr;   // never invent a level root
    for (size_t i = 1; i < parts.size(); ++i){
        GameObject* next = childNamed(node, parts[i]);
        if (!next){ next = scene->createGameObject(parts[i], node); ++created; }
        node = next;
    }
    return node;
}

Vector3 vec3(const Value& o, const char* key, const Vector3& fallback){
    if (!o.HasMember(key) || !o[key].IsArray() || o[key].Size() < 3) return fallback;
    const Value& v = o[key];
    return Vector3(v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat());
}

std::string str(const Value& o, const char* key){
    return o.HasMember(key) && o[key].IsString() ? o[key].GetString() : std::string();
}

ComponentScript* scriptOf(GameObject* go, const std::string& cls){
    for (const auto& c : go->getComponents())
        if (c->getType() == Component::Type::Script && static_cast<ComponentScript*>(c.get())->getClassName() == cls)
            return static_cast<ComponentScript*>(c.get());
    return nullptr;
}

// Writes `fields` over the component's saved fields (its own onSave / onLoad round trip).
void writeFields(ComponentScript* cs, const Value& fields){
    std::string json;
    cs->onSave(json);
    rapidjson::Document d;
    d.Parse(json.c_str());
    if (d.HasParseError() || !d.IsObject()) return;
    auto& a = d.GetAllocator();
    if (!d.HasMember("Fields") || !d["Fields"].IsObject()) d.AddMember("Fields", Value(rapidjson::kObjectType), a);
    Value& f = d["Fields"];
    for (auto it = fields.MemberBegin(); it != fields.MemberEnd(); ++it){
        Value copy(it->value, a);
        auto m = f.FindMember(it->name.GetString());
        if (m != f.MemberEnd()) m->value = copy;
        else f.AddMember(Value(it->name.GetString(), a), copy, a);
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    d.Accept(w);
    cs->onLoad(buf.GetString());
}

GameObject* outermostPrefabRoot(GameObject* go){
    GameObject* root = nullptr;
    for (GameObject* n = go; n; n = n->getParent()) if (PrefabManager::isPrefabInstance(n)) root = n;
    return root;
}

} // namespace

bool SetUpAshfallTrapsPuzzles(SceneManager* sm, HotReloadManager* hotReload, std::string& outMessage){
    SceneGraph* scene = sm ? sm->getModuleScene() : nullptr;
    if (!scene){ outMessage = "no active scene"; return false; }
    const std::string current = sm->getCurrentScenePath();
    const std::string sceneName = std::filesystem::path(current).stem().string();
    const std::string level = sceneName.rfind("AF_", 0) == 0 ? sceneName.substr(3) : sceneName;
    const std::string file = app->getFileSystem()->GetAssetsPath() + std::string("Traps/setup/") + level + ".setup.json";

    std::ifstream in(file, std::ios::binary);
    if (sceneName.empty() || !in){ outMessage = "no setup file for '" + (sceneName.empty() ? std::string("an unsaved scene") : sceneName) + "' (" + file + ")"; return false; }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(text.c_str());
    if (doc.HasParseError() || !doc.IsObject()){ outMessage = file + " is not valid JSON"; return false; }

    const std::vector<std::string> classes = hotReload ? hotReload->getRegisteredClassNames() : std::vector<std::string>();
    if (doc.HasMember("scripts") && doc["scripts"].IsArray())
        for (const Value& s : doc["scripts"].GetArray()){
            const std::string cls = str(s, "class");
            if (std::find(classes.begin(), classes.end(), cls) == classes.end()){
                outMessage = "script class '" + cls + "' is not loaded: build GameScript.dll first";
                return false;
            }
        }

    std::vector<GameObject*> touched;
    int renamed = 0, created = 0, groups = 0, scripts = 0;
    std::string problems;

    if (doc.HasMember("rename") && doc["rename"].IsArray())
        for (const Value& r : doc["rename"].GetArray()){
            GameObject* parent = findPath(scene, str(r, "path"));
            if (!parent){ problems += " [rename: no " + str(r, "path") + "]"; continue; }
            if (childNamed(parent, str(r, "to"))) continue;   // done before
            const int index = r.HasMember("index") && r["index"].IsInt() ? r["index"].GetInt() : 0;
            GameObject* go = childNamed(parent, str(r, "name"), index);
            if (!go){ problems += " [rename: no " + str(r, "name") + " #" + std::to_string(index) + "]"; continue; }
            go->setName(str(r, "to"));
            touched.push_back(go);
            ++renamed;
        }

    const std::string assets = app->getFileSystem()->GetAssetsPath();
    if (doc.HasMember("create") && doc["create"].IsArray())
        for (const Value& c : doc["create"].GetArray()){
            const std::string name = str(c, "name");
            GameObject* parent = ensurePath(scene, str(c, "parent"), groups);
            if (!parent){ problems += " [create " + name + ": no root for " + str(c, "parent") + "]"; continue; }
            if (childNamed(parent, name)) continue;   // already there: keep hand edits
            GameObject* go = scene->createGameObject(name, parent);
            ComponentTransform* t = go->getTransform();
            t->position = vec3(c, "position", Vector3::Zero);
            t->scale = vec3(c, "scale", Vector3::One);
            if (c.HasMember("rotation") && c["rotation"].IsArray() && c["rotation"].Size() == 4){
                const Value& q = c["rotation"];
                t->rotation = Quaternion(q[0].GetFloat(), q[1].GetFloat(), q[2].GetFloat(), q[3].GetFloat());
            }
            else if (c.HasMember("yaw") && c["yaw"].IsNumber())
                t->rotation = Quaternion::CreateFromYawPitchRoll(c["yaw"].GetFloat() * 3.14159265f / 180.f, 0.f, 0.f);
            t->markDirty();
            if (!str(c, "tag").empty()) go->setTag(str(c, "tag"));
            const std::string model = str(c, "model");
            if (!model.empty()){
                go->addComponent(ComponentFactory::CreateComponent(Component::Type::Mesh, go));
                ComponentMesh* cm = go->getComponent<ComponentMesh>();
                if (!cm || !cm->loadModel((assets + model).c_str())) problems += " [" + name + ": model " + model + " not found]";
            }
            touched.push_back(go);
            ++created;
        }

    if (doc.HasMember("scripts") && doc["scripts"].IsArray())
        for (const Value& s : doc["scripts"].GetArray()){
            const std::string path = str(s, "object"), cls = str(s, "class");
            GameObject* go = findPath(scene, path);
            if (!go){ problems += " [" + cls + ": no " + path + "]"; continue; }
            ComponentScript* cs = scriptOf(go, cls);
            if (!cs){
                auto comp = ComponentFactory::CreateComponent(Component::Type::Script, go);
                comp->onLoad("{\"ClassName\":\"" + cls + "\"}");
                cs = static_cast<ComponentScript*>(comp.get());
                go->addComponent(std::move(comp));
            }
            if (s.HasMember("fields") && s["fields"].IsObject()) writeFields(cs, s["fields"]);
            touched.push_back(go);
            ++scripts;
        }

    // The editor's Apply for each prefab instance these objects live in, so the prefab carries them too.
    std::vector<GameObject*> applied;
    for (GameObject* go : touched){
        GameObject* root = outermostPrefabRoot(go);
        if (!root || std::find(applied.begin(), applied.end(), root) != applied.end()) continue;
        if (PrefabManager::applyToPrefab(root)) applied.push_back(root);
    }
    std::string done = std::to_string(scripts) + " scripts, " + std::to_string(created) + " placeholders (" + std::to_string(groups)
        + " groups), " + std::to_string(renamed) + " renamed";
    for (GameObject* r : applied) done += "; applied to prefab " + PrefabManager::getPrefabName(r);
    if (!problems.empty()) done += "; PROBLEMS:" + problems;

    if (!sm->saveCurrentScene(current)){ outMessage = "wired (" + done + ") but saving " + current + " failed: save it by hand"; return false; }
    outMessage = done + "; saved " + current;
    return problems.empty();
}
