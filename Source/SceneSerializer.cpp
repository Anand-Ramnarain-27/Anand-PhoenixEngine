#include "Globals.h"
#include "SceneSerializer.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentFactory.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/filereadstream.h"
#include "3rdParty/rapidjson/filewritestream.h"

using namespace rapidjson;

#include <functional>
#include <unordered_map>
#include <cstdio>

bool SceneSerializer::SaveScene(const ModuleScene* scene, const std::string& filePath)
{
    if (!scene)
        return false;

    LOG("SceneSerializer: Saving scene to %s", filePath.c_str());

    try {
        // Create JSON document
        Document doc;
        doc.SetObject();
        Document::AllocatorType& allocator = doc.GetAllocator();

        // Create Scene object
        Value sceneObj(kObjectType);
        sceneObj.AddMember("Version", 1, allocator);

        // Create GameObjects array
        Value gameObjectsArray(kArrayType);

        // Recursive serialization function
        std::function<void(GameObject*, Value&)> serializeGO =
            [&](GameObject* go, Value& arr)
            {
                // Skip root itself
                if (go == scene->getRoot())
                    return;

                Value goNode(kObjectType);

                // Basic properties
                goNode.AddMember("UID", go->getUID(), allocator);
                goNode.AddMember("ParentUID",
                    go->getParent() ? go->getParent()->getUID() : 0,
                    allocator);

                Value nameVal;
                nameVal.SetString(go->getName().c_str(), go->getName().length(), allocator);
                goNode.AddMember("Name", nameVal, allocator);

                goNode.AddMember("Active", go->isActive(), allocator);

                // Serialize Transform - stored inline with GameObject
                auto* transform = go->getTransform();
                Value transformObj(kObjectType);

                // Position
                {
                    Value posArray(kArrayType);
                    posArray.PushBack(Value(transform->position.x), allocator);
                    posArray.PushBack(Value(transform->position.y), allocator);
                    posArray.PushBack(Value(transform->position.z), allocator);
                    transformObj.AddMember("position", posArray, allocator);
                }

                // Rotation
                {
                    Value rotArray(kArrayType);
                    rotArray.PushBack(Value(transform->rotation.x), allocator);
                    rotArray.PushBack(Value(transform->rotation.y), allocator);
                    rotArray.PushBack(Value(transform->rotation.z), allocator);
                    rotArray.PushBack(Value(transform->rotation.w), allocator);
                    transformObj.AddMember("rotation", rotArray, allocator);
                }

                // Scale
                {
                    Value scaleArray(kArrayType);
                    scaleArray.PushBack(Value(transform->scale.x), allocator);
                    scaleArray.PushBack(Value(transform->scale.y), allocator);
                    scaleArray.PushBack(Value(transform->scale.z), allocator);
                    transformObj.AddMember("scale", scaleArray, allocator);
                }

                goNode.AddMember("Transform", transformObj, allocator);

                // ? FIXED: Serialize Components (excluding Transform which is always present)
                Value componentsArray(kArrayType);

                for (const auto& comp : go->getComponents())
                {
                    // Skip Transform - already serialized above
                    if (comp->getType() == Component::Type::Transform)
                        continue;

                    Value compObj(kObjectType);

                    // Store component type
                    compObj.AddMember("Type", (int)comp->getType(), allocator);

                    // Call component's onSave method
                    std::string compData;
                    comp->onSave(compData);

                    // Store serialized data
                    Value dataVal;
                    dataVal.SetString(compData.c_str(), compData.length(), allocator);
                    compObj.AddMember("Data", dataVal, allocator);

                    componentsArray.PushBack(compObj, allocator);
                }

                goNode.AddMember("Components", componentsArray, allocator);

                arr.PushBack(goNode, allocator);

                // Recurse children
                for (auto* child : go->getChildren()) {
                    serializeGO(child, arr);
                }
            };

        // Start from root's children
        for (auto* child : scene->getRoot()->getChildren()) {
            serializeGO(child, gameObjectsArray);
        }

        // Store count before moving
        uint32_t objectCount = gameObjectsArray.Size();

        sceneObj.AddMember("GameObjects", gameObjectsArray, allocator);
        doc.AddMember("Scene", sceneObj, allocator);

        // Write to file with pretty formatting
        FILE* fp = fopen(filePath.c_str(), "wb");
        if (!fp) {
            LOG("SceneSerializer: Failed to open file for writing: %s", filePath.c_str());
            return false;
        }

        char writeBuffer[65536];
        FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

        PrettyWriter<FileWriteStream> writer(os);
        writer.SetIndent(' ', 2);
        doc.Accept(writer);

        fclose(fp);

        LOG("SceneSerializer: Scene saved successfully (%d objects)", objectCount);
        return true;

    }
    catch (const std::exception& e) {
        LOG("SceneSerializer: EXCEPTION during save: %s", e.what());
        return false;
    }
    catch (...) {
        LOG("SceneSerializer: UNKNOWN EXCEPTION during save");
        return false;
    }
}

bool SceneSerializer::LoadScene(const std::string& filePath, ModuleScene* scene)
{
    if (!scene)
        return false;

    LOG("SceneSerializer: Loading scene from %s", filePath.c_str());

    ModuleFileSystem* fs = app->getFileSystem();

    if (!fs->Exists(filePath.c_str())) {
        LOG("SceneSerializer: File does not exist: %s", filePath.c_str());
        return false;
    }

    // Read file
    FILE* fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        LOG("SceneSerializer: Failed to open file for reading: %s", filePath.c_str());
        return false;
    }

    char readBuffer[65536];
    FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    // Parse JSON
    Document doc;
    doc.ParseStream(is);
    fclose(fp);

    if (doc.HasParseError()) {
        LOG("SceneSerializer: JSON parse error at offset %u: %d",
            (unsigned)doc.GetErrorOffset(), doc.GetParseError());
        return false;
    }

    if (!doc.HasMember("Scene") || !doc["Scene"].HasMember("GameObjects")) {
        LOG("SceneSerializer: Invalid scene file format");
        return false;
    }

    const Value& gameObjectsArray = doc["Scene"]["GameObjects"];

    // Clear existing scene (except root)
    scene->clear();

    // PHASE 1: Create all GameObjects (without linking or loading components)
    std::unordered_map<uint32_t, GameObject*> uidMap;

    for (SizeType i = 0; i < gameObjectsArray.Size(); i++) {
        const Value& goNode = gameObjectsArray[i];

        std::string name = goNode["Name"].GetString();
        bool active = goNode["Active"].GetBool();

        // Create GameObject (this auto-creates Transform component)
        GameObject* go = scene->createGameObject(name);
        go->setActive(active);

        // Store UID mapping for phase 2
        uint32_t savedUID = goNode["UID"].GetUint();
        uidMap[savedUID] = go;
    }

    LOG("SceneSerializer: Created %d GameObjects", (int)uidMap.size());

    // PHASE 2: Link parents and load all data
    for (SizeType i = 0; i < gameObjectsArray.Size(); i++) {
        const Value& goNode = gameObjectsArray[i];

        uint32_t uid = goNode["UID"].GetUint();
        uint32_t parentUID = goNode["ParentUID"].GetUint();

        GameObject* go = uidMap[uid];

        // Set parent (skip if parent is root = 0)
        if (parentUID != 0) {
            if (uidMap.count(parentUID)) {
                go->setParent(uidMap[parentUID]);
            }
            else {
                LOG("SceneSerializer: Warning - Parent UID %u not found for GameObject %s",
                    parentUID, go->getName().c_str());
            }
        }

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

        // ? FIXED: Load Components using addComponent method
        const Value& componentsArray = goNode["Components"];
        for (SizeType j = 0; j < componentsArray.Size(); j++) {
            const Value& compNode = componentsArray[j];

            // Get component type
            Component::Type type = (Component::Type)compNode["Type"].GetInt();

            // Get serialized data
            std::string compData = compNode["Data"].GetString();

            // Create component using factory
            auto component = ComponentFactory::CreateComponent(type, go);

            if (component)
            {
                // Load component data BEFORE adding to GameObject
                component->onLoad(compData);

                // ? Add component to GameObject
                go->addComponent(std::move(component));

                LOG("SceneSerializer: Loaded component type %d for GameObject %s",
                    (int)type, go->getName().c_str());
            }
            else
            {
                LOG("SceneSerializer: Failed to create component type %d", (int)type);
            }
        }
    }

    LOG("SceneSerializer: Scene loaded successfully");
    return true;
}

bool SceneSerializer::SaveTempScene(const ModuleScene* scene)
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string tempPath = fs->GetLibraryPath() + "Scenes/temp_scene.json";

    LOG("SceneSerializer: Saving temporary scene state");
    return SaveScene(scene, tempPath);
}

bool SceneSerializer::LoadTempScene(ModuleScene* scene)
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string tempPath = fs->GetLibraryPath() + "Scenes/temp_scene.json";

    LOG("SceneSerializer: Loading temporary scene state");
    return LoadScene(tempPath, scene);
}

std::string SceneSerializer::SerializeGameObject(const GameObject* go)
{
    // Not used - kept for potential future use
    return "";
}

GameObject* SceneSerializer::DeserializeGameObject(const std::string& json, ModuleScene* scene)
{
    // Not used - kept for potential future use
    return nullptr;
}