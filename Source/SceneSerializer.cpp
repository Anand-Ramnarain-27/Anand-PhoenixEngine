#include "Globals.h"
#include "SceneSerializer.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentFactory.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <functional>
#include <unordered_map>

using namespace rapidjson;

static void pushVec3(Value& arr, const Vector3& v, Document::AllocatorType& a)
{
    arr.PushBack(v.x, a).PushBack(v.y, a).PushBack(v.z, a);
}

static void pushQuat(Value& arr, const Quaternion& q, Document::AllocatorType& a)
{
    arr.PushBack(q.x, a).PushBack(q.y, a).PushBack(q.z, a).PushBack(q.w, a);
}

bool SceneSerializer::SaveScene(const ModuleScene* scene, const std::string& filePath)
{
    if (!scene) return false;

    try
    {
        Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
        Value sceneObj(kObjectType);
        sceneObj.AddMember("Version", 1, a);

        Value goArray(kArrayType);

        std::function<void(GameObject*)> serialize = [&](GameObject* go)
            {
                if (go == scene->getRoot()) return;

                Value node(kObjectType);

                node.AddMember("UID", go->getUID(), a);
                node.AddMember("ParentUID",
                    go->getParent() ? go->getParent()->getUID() : 0u, a);

                node.AddMember("Name", Value(go->getName().c_str(), a), a);
                node.AddMember("Active", go->isActive(), a);

                auto* t = go->getTransform();
                Value tf(kObjectType);
                Value pos(kArrayType); pushVec3(pos, t->position, a); tf.AddMember("position", pos, a);
                Value rot(kArrayType); pushQuat(rot, t->rotation, a); tf.AddMember("rotation", rot, a);
                Value scl(kArrayType); pushVec3(scl, t->scale, a); tf.AddMember("scale", scl, a);
                node.AddMember("Transform", tf, a);

                Value comps(kArrayType);
                for (const auto& comp : go->getComponents())
                {
                    if (comp->getType() == Component::Type::Transform) continue;
                    std::string data;
                    comp->onSave(data);
                    Value c(kObjectType);
                    c.AddMember("Type", (int)comp->getType(), a);
                    c.AddMember("Data", Value(data.c_str(), a), a);
                    comps.PushBack(c, a);
                }
                node.AddMember("Components", comps, a);

                goArray.PushBack(node, a);
                for (auto* child : go->getChildren()) serialize(child);
            };

        for (auto* child : scene->getRoot()->getChildren()) serialize(child);

        sceneObj.AddMember("GameObjects", goArray, a);
        doc.AddMember("Scene", sceneObj, a);

        StringBuffer sb;
        PrettyWriter<StringBuffer> writer(sb);
        doc.Accept(writer);

        return app->getFileSystem()->Save(filePath.c_str(), sb.GetString(), (unsigned)sb.GetSize());
    }
    catch (const std::exception& e) { LOG("SceneSerializer: Save exception: %s", e.what()); return false; }
    catch (...) { LOG("SceneSerializer: Unknown save exception");        return false; }
}

bool SceneSerializer::LoadScene(const std::string& filePath, ModuleScene* scene)
{
    if (!scene) return false;

    auto* fs = app->getFileSystem();
    if (!fs->Exists(filePath.c_str()))
    {
        LOG("SceneSerializer: File not found: %s", filePath.c_str());
        return false;
    }

    char* buf = nullptr;
    unsigned size = fs->Load(filePath.c_str(), &buf);
    if (!buf || size == 0) return false;

    Document doc;
    doc.Parse(buf, size);
    delete[] buf;

    if (doc.HasParseError() || !doc.HasMember("Scene") || !doc["Scene"].HasMember("GameObjects"))
    {
        LOG("SceneSerializer: Invalid file or parse error");
        return false;
    }

    const Value& goArray = doc["Scene"]["GameObjects"];

    scene->clear();

    std::unordered_map<uint32_t, GameObject*> uidMap;

    for (SizeType i = 0; i < goArray.Size(); ++i)
    {
        const Value& node = goArray[i];

        uint32_t uid = 0;
        if (node["UID"].IsUint())       uid = node["UID"].GetUint();
        else if (node["UID"].IsInt())   uid = static_cast<uint32_t>(node["UID"].GetInt());
        else { LOG("SceneSerializer: UID has unexpected type, skipping"); continue; }

        auto* go = scene->createGameObject(node["Name"].GetString());
        go->setActive(node["Active"].GetBool());
        uidMap[uid] = go;
    }

    for (SizeType i = 0; i < goArray.Size(); ++i)
    {
        const Value& node = goArray[i];

        uint32_t uid = 0;
        if (node["UID"].IsUint()) uid = node["UID"].GetUint();
        else if (node["UID"].IsInt())  uid = static_cast<uint32_t>(node["UID"].GetInt());
        else continue;

        auto it = uidMap.find(uid);
        if (it == uidMap.end()) continue;
        auto* go = it->second;

        uint32_t parentID = 0;
        if (node["ParentUID"].IsUint()) parentID = node["ParentUID"].GetUint();
        else if (node["ParentUID"].IsInt())  parentID = static_cast<uint32_t>(node["ParentUID"].GetInt());

        if (parentID != 0)
        {
            auto pit = uidMap.find(parentID);
            if (pit != uidMap.end()) go->setParent(pit->second);
            else LOG("SceneSerializer: Parent UID %u not found for %s", parentID, go->getName().c_str());
        }

        auto* t = go->getTransform();
        const Value& tf = node["Transform"];
        const auto& p = tf["position"]; t->position = { p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat() };
        const auto& r = tf["rotation"]; t->rotation = { r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat(), r[3].GetFloat() };
        const auto& s = tf["scale"];    t->scale = { s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat() };
        t->markDirty();

        for (SizeType j = 0; j < node["Components"].Size(); ++j)
        {
            const Value& cn = node["Components"][j];
            auto          type = (Component::Type)cn["Type"].GetInt();
            auto          comp = ComponentFactory::CreateComponent(type, go);
            if (comp)
            {
                comp->onLoad(cn["Data"].GetString());
                go->addComponent(std::move(comp));
            }
            else
            {
                LOG("SceneSerializer: Failed to create component type %d", (int)type);
            }
        }
    }

    return true;
}

bool SceneSerializer::SaveTempScene(const ModuleScene* scene)
{
    return SaveScene(scene, app->getFileSystem()->GetLibraryPath() + "Scenes/temp_scene.json");
}

bool SceneSerializer::LoadTempScene(ModuleScene* scene)
{
    return LoadScene(app->getFileSystem()->GetLibraryPath() + "Scenes/temp_scene.json", scene);
}