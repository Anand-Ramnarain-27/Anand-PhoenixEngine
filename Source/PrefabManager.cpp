#include "Globals.h"
#include "PrefabManager.h"
#include "GameObject.h"
#include "ModuleScene.h"
#include "ComponentTransform.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/filereadstream.h"
#include "3rdParty/rapidjson/filewritestream.h"

#include <filesystem>

using namespace rapidjson;

std::string PrefabManager::getPrefabPath(const std::string& prefabName)
{
    return "Library/Prefabs/" + prefabName + ".prefab";
}

bool PrefabManager::createPrefab(const GameObject* go, const std::string& prefabName)
{
    if (!go || prefabName.empty())
        return false;

    // Ensure directory exists
    app->getFileSystem()->CreateDir("Library/Prefabs");

    std::string prefabPath = getPrefabPath(prefabName);
    LOG("PrefabManager: Creating prefab '%s'", prefabName.c_str());

    // Create JSON document
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    // Prefab metadata
    doc.AddMember("PrefabName", Value(prefabName.c_str(), allocator), allocator);
    doc.AddMember("Version", 1, allocator);

    // GameObject data
    Value goNode(kObjectType);

    // Name
    Value nameVal(go->getName().c_str(), allocator);
    goNode.AddMember("Name", nameVal, allocator);

    // Active
    goNode.AddMember("Active", go->isActive(), allocator);

    // Transform
    auto* transform = go->getTransform();
    Value transformObj(kObjectType);

    {
        Value posArray(kArrayType);
        posArray.PushBack(Value(transform->position.x), allocator);
        posArray.PushBack(Value(transform->position.y), allocator);
        posArray.PushBack(Value(transform->position.z), allocator);
        transformObj.AddMember("position", posArray, allocator);
    }

    {
        Value rotArray(kArrayType);
        rotArray.PushBack(Value(transform->rotation.x), allocator);
        rotArray.PushBack(Value(transform->rotation.y), allocator);
        rotArray.PushBack(Value(transform->rotation.z), allocator);
        rotArray.PushBack(Value(transform->rotation.w), allocator);
        transformObj.AddMember("rotation", rotArray, allocator);
    }

    {
        Value scaleArray(kArrayType);
        scaleArray.PushBack(Value(transform->scale.x), allocator);
        scaleArray.PushBack(Value(transform->scale.y), allocator);
        scaleArray.PushBack(Value(transform->scale.z), allocator);
        transformObj.AddMember("scale", scaleArray, allocator);
    }

    goNode.AddMember("Transform", transformObj, allocator);

    // Components (empty for now - extend later)
    Value componentsArray(kArrayType);
    goNode.AddMember("Components", componentsArray, allocator);

    doc.AddMember("GameObject", goNode, allocator);

    // Write to file
    FILE* fp = fopen(prefabPath.c_str(), "wb");
    if (!fp)
    {
        LOG("PrefabManager: Failed to open file");
        return false;
    }

    char writeBuffer[65536];
    FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
    PrettyWriter<FileWriteStream> writer(os);
    writer.SetIndent(' ', 2);
    doc.Accept(writer);
    fclose(fp);

    LOG("PrefabManager: Prefab created successfully");
    return true;
}

GameObject* PrefabManager::instantiatePrefab(const std::string& prefabName, ModuleScene* scene)
{
    if (!scene || prefabName.empty())
        return nullptr;

    std::string prefabPath = getPrefabPath(prefabName);

    if (!app->getFileSystem()->Exists(prefabPath.c_str()))
    {
        LOG("PrefabManager: Prefab not found: %s", prefabPath.c_str());
        return nullptr;
    }

    // Read file
    FILE* fp = fopen(prefabPath.c_str(), "rb");
    if (!fp)
        return nullptr;

    char readBuffer[65536];
    FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    Document doc;
    doc.ParseStream(is);
    fclose(fp);

    if (doc.HasParseError() || !doc.HasMember("GameObject"))
        return nullptr;

    const Value& goNode = doc["GameObject"];

    // Create GameObject
    std::string name = goNode["Name"].GetString();
    bool active = goNode["Active"].GetBool();

    GameObject* go = scene->createGameObject(name);
    go->setActive(active);

    // Load Transform
    const Value& transformObj = goNode["Transform"];
    auto* transform = go->getTransform();

    const Value& pos = transformObj["position"];
    transform->position = Vector3(
        pos[0].GetFloat(),
        pos[1].GetFloat(),
        pos[2].GetFloat()
    );

    const Value& rot = transformObj["rotation"];
    transform->rotation = Quaternion(
        rot[0].GetFloat(),
        rot[1].GetFloat(),
        rot[2].GetFloat(),
        rot[3].GetFloat()
    );

    const Value& scale = transformObj["scale"];
    transform->scale = Vector3(
        scale[0].GetFloat(),
        scale[1].GetFloat(),
        scale[2].GetFloat()
    );

    transform->markDirty();

    LOG("PrefabManager: Instantiated prefab '%s'", prefabName.c_str());
    return go;
}

std::vector<std::string> PrefabManager::listPrefabs()
{
    std::vector<std::string> prefabs;
    std::string prefabsPath = "Library/Prefabs/";

    if (!app->getFileSystem()->Exists(prefabsPath.c_str()))
        return prefabs;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(prefabsPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".prefab")
            {
                prefabs.push_back(entry.path().stem().string());
            }
        }
    }
    catch (...) {}

    return prefabs;
}