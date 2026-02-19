#include "Globals.h"
#include "PrefabManager.h"
#include "GameObject.h"
#include "ModuleScene.h"
#include "ComponentTransform.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/filereadstream.h"
#include "3rdParty/rapidjson/filewritestream.h"
#include <filesystem>

using namespace rapidjson;

std::string PrefabManager::getPrefabPath(const std::string& name)
{
    return "Library/Prefabs/" + name + ".prefab";
}

bool PrefabManager::createPrefab(const GameObject* go, const std::string& prefabName)
{
    if (!go || prefabName.empty()) return false;

    app->getFileSystem()->CreateDir("Library/Prefabs");

    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("PrefabName", Value(prefabName.c_str(), a), a);
    doc.AddMember("Version", 1, a);

    Value goNode(kObjectType);
    goNode.AddMember("Name", Value(go->getName().c_str(), a), a);
    goNode.AddMember("Active", go->isActive(), a);

    auto* t = go->getTransform();
    Value tf(kObjectType);

    Value pos(kArrayType); pos.PushBack(t->position.x, a).PushBack(t->position.y, a).PushBack(t->position.z, a);
    Value rot(kArrayType); rot.PushBack(t->rotation.x, a).PushBack(t->rotation.y, a).PushBack(t->rotation.z, a).PushBack(t->rotation.w, a);
    Value scl(kArrayType); scl.PushBack(t->scale.x, a).PushBack(t->scale.y, a).PushBack(t->scale.z, a);

    tf.AddMember("position", pos, a);
    tf.AddMember("rotation", rot, a);
    tf.AddMember("scale", scl, a);
    goNode.AddMember("Transform", tf, a);
    goNode.AddMember("Components", Value(kArrayType), a);
    doc.AddMember("GameObject", goNode, a);

    FILE* fp = fopen(getPrefabPath(prefabName).c_str(), "wb");
    if (!fp) { LOG("PrefabManager: Failed to open file"); return false; }

    char buf[65536];
    FileWriteStream os(fp, buf, sizeof(buf));
    PrettyWriter<FileWriteStream> writer(os);
    writer.SetIndent(' ', 2);
    doc.Accept(writer);
    fclose(fp);
    return true;
}

GameObject* PrefabManager::instantiatePrefab(const std::string& prefabName, ModuleScene* scene)
{
    if (!scene || prefabName.empty()) return nullptr;

    std::string path = getPrefabPath(prefabName);
    if (!app->getFileSystem()->Exists(path.c_str())) { LOG("PrefabManager: Not found: %s", path.c_str()); return nullptr; }

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return nullptr;

    char buf[65536];
    FileReadStream is(fp, buf, sizeof(buf));
    Document doc; doc.ParseStream(is);
    fclose(fp);

    if (doc.HasParseError() || !doc.HasMember("GameObject")) return nullptr;

    const Value& node = doc["GameObject"];
    GameObject* go = scene->createGameObject(node["Name"].GetString());
    go->setActive(node["Active"].GetBool());

    auto* t = go->getTransform();
    const Value& tf = node["Transform"];
    const auto& p = tf["position"]; t->position = { p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat() };
    const auto& r = tf["rotation"]; t->rotation = { r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat(), r[3].GetFloat() };
    const auto& s = tf["scale"];    t->scale = { s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat() };
    t->markDirty();

    return go;
}

std::vector<std::string> PrefabManager::listPrefabs()
{
    std::vector<std::string> prefabs;
    const std::string dir = "Library/Prefabs/";
    if (!app->getFileSystem()->Exists(dir.c_str())) return prefabs;

    try {
        for (const auto& e : std::filesystem::directory_iterator(dir))
            if (e.is_regular_file() && e.path().extension() == ".prefab")
                prefabs.push_back(e.path().stem().string());
    }
    catch (...) {}

    return prefabs;
}