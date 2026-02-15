#include "Globals.h"
#include "ComponentTransform.h"
#include "GameObject.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentTransform::ComponentTransform(GameObject* owner)
    : Component(owner)
{
}

void ComponentTransform::markDirty()
{
    dirty = true;

    for (auto* child : owner->getChildren())
    {
        if (child->getTransform())
            child->getTransform()->markDirty();
    }
}

void ComponentTransform::rebuildLocal()
{
    localMatrix =
        Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(rotation) *
        Matrix::CreateTranslation(position);
}

void ComponentTransform::rebuildGlobal()
{
    if (auto parent = owner->getParent())
    {
        globalMatrix =
            parent->getTransform()->getGlobalMatrix() * localMatrix;
    }
    else
    {
        globalMatrix = localMatrix;
    }
}

const Matrix& ComponentTransform::getLocalMatrix()
{
    if (dirty)
    {
        rebuildLocal();
        dirty = false;
    }
    return localMatrix;
}

const Matrix& ComponentTransform::getGlobalMatrix()
{
    if (dirty)
    {
        rebuildLocal();
        rebuildGlobal();
        dirty = false;
    }
    return globalMatrix;
}

void ComponentTransform::onSave(std::string& outJson) const
{
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    // Position
    Value posArray(kArrayType);
    posArray.PushBack(position.x, allocator);
    posArray.PushBack(position.y, allocator);
    posArray.PushBack(position.z, allocator);
    doc.AddMember("position", posArray, allocator);

    // Rotation
    Value rotArray(kArrayType);
    rotArray.PushBack(rotation.x, allocator);
    rotArray.PushBack(rotation.y, allocator);
    rotArray.PushBack(rotation.z, allocator);
    rotArray.PushBack(rotation.w, allocator);
    doc.AddMember("rotation", rotArray, allocator);

    // Scale
    Value scaleArray(kArrayType);
    scaleArray.PushBack(scale.x, allocator);
    scaleArray.PushBack(scale.y, allocator);
    scaleArray.PushBack(scale.z, allocator);
    doc.AddMember("scale", scaleArray, allocator);

    // Convert to string
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    outJson = buffer.GetString();
}

void ComponentTransform::onLoad(const std::string& jsonStr)
{
    Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError()) {
        LOG("ComponentTransform: JSON parse error");
        return;
    }

    // Load position
    if (doc.HasMember("position")) {
        const Value& pos = doc["position"];
        position = Vector3(
            pos[0].GetFloat(),
            pos[1].GetFloat(),
            pos[2].GetFloat()
        );
    }

    // Load rotation
    if (doc.HasMember("rotation")) {
        const Value& rot = doc["rotation"];
        rotation = Quaternion(
            rot[0].GetFloat(),
            rot[1].GetFloat(),
            rot[2].GetFloat(),
            rot[3].GetFloat()
        );
    }

    // Load scale
    if (doc.HasMember("scale")) {
        const Value& scl = doc["scale"];
        scale = Vector3(
            scl[0].GetFloat(),
            scl[1].GetFloat(),
            scl[2].GetFloat()
        );
    }

    markDirty();
}