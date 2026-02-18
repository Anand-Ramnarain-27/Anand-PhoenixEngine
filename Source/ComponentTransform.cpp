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
        if (auto* t = child->getTransform()) t->markDirty();
}

void ComponentTransform::rebuildLocal()
{
    localMatrix = Matrix::CreateScale(scale)
        * Matrix::CreateFromQuaternion(rotation)
        * Matrix::CreateTranslation(position);
}

void ComponentTransform::rebuildGlobal()
{
    auto* parent = owner->getParent();
    globalMatrix = parent ? parent->getTransform()->getGlobalMatrix() * localMatrix : localMatrix;
}

const Matrix& ComponentTransform::getLocalMatrix()
{
    if (dirty) { rebuildLocal(); dirty = false; }
    return localMatrix;
}

const Matrix& ComponentTransform::getGlobalMatrix()
{
    if (dirty) { rebuildLocal(); rebuildGlobal(); dirty = false; }
    return globalMatrix;
}

void ComponentTransform::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();

    Value pos(kArrayType); pos.PushBack(position.x, a).PushBack(position.y, a).PushBack(position.z, a);
    Value rot(kArrayType); rot.PushBack(rotation.x, a).PushBack(rotation.y, a).PushBack(rotation.z, a).PushBack(rotation.w, a);
    Value scl(kArrayType); scl.PushBack(scale.x, a).PushBack(scale.y, a).PushBack(scale.z, a);

    doc.AddMember("position", pos, a);
    doc.AddMember("rotation", rot, a);
    doc.AddMember("scale", scl, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentTransform::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) { LOG("ComponentTransform: JSON parse error"); return; }

    if (doc.HasMember("position")) { auto& p = doc["position"]; position = { p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat() }; }
    if (doc.HasMember("rotation")) { auto& r = doc["rotation"]; rotation = { r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat(), r[3].GetFloat() }; }
    if (doc.HasMember("scale")) { auto& s = doc["scale"];    scale = { s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat() }; }

    markDirty();
}