#include "Globals.h"
#include "PrefabManager.h"
#include "GameObject.h"
#include "ModuleScene.h"
#include "ComponentTransform.h"
#include "ComponentFactory.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include "3rdParty/rapidjson/filereadstream.h"
#include "3rdParty/rapidjson/filewritestream.h"
#include "3rdParty/rapidjson/error/en.h"

#include <filesystem>
#include <algorithm>
#include <cassert>

using namespace rapidjson;
namespace fs = std::filesystem;

static constexpr int kPrefabFormatVersion = 2;
static constexpr char kPrefabDir[] = "Library/Prefabs/";
static constexpr char kPrefabExt[] = ".prefab";

struct PrefabManager::SerialiseCtx {
    Document& doc;
    Document::AllocatorType& a;
    explicit SerialiseCtx(Document& d) : doc(d), a(d.GetAllocator()) {}
};

std::unordered_map<const GameObject*, PrefabInstanceData>& PrefabManager::registry() {
    static std::unordered_map<const GameObject*, PrefabInstanceData> s_registry;
    return s_registry;
}

uint32_t PrefabManager::makePrefabUID(const std::string& name) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : name) { hash ^= c; hash *= 16777619u; }
    return hash ? hash : 1u;
}

std::string PrefabManager::getPrefabPath(const std::string& name) { return std::string(kPrefabDir) + name + kPrefabExt; }

bool PrefabManager::writePrefabDocument(Document& doc, const std::string& path) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    char buf[65536];
    FileWriteStream os(fp, buf, sizeof(buf));
    PrettyWriter<FileWriteStream> writer(os);
    writer.SetIndent(' ', 2);
    doc.Accept(writer);
    fclose(fp);
    return true;
}

bool PrefabManager::readPrefabDocument(const std::string& path, Document& doc) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    char buf[65536];
    FileReadStream is(fp, buf, sizeof(buf));
    doc.ParseStream(is);
    fclose(fp);
    return !doc.HasParseError();
}

void PrefabManager::serialiseNode(const GameObject* go, SerialiseCtx& ctx) {
    (void)go; (void)ctx;
    assert(false && "Call serialiseNode overload with Value& out instead");
}

static void serialiseNodeInto(const GameObject* go, Value& out, Document::AllocatorType& a) {
    out.SetObject();
    out.AddMember("Name", Value(go->getName().c_str(), a), a);
    out.AddMember("UID", go->getUID(), a);
    out.AddMember("Active", go->isActive(), a);

    const PrefabInstanceData* instData = PrefabManager::getInstanceData(go);
    if (instData) {
        Value linkNode(kObjectType);
        linkNode.AddMember("PrefabName", Value(instData->prefabName.c_str(), a), a);
        linkNode.AddMember("PrefabUID", instData->prefabUID, a);
        out.AddMember("PrefabLink", linkNode, a);
    }

    auto* t = go->getTransform();
    Value tf(kObjectType);
    Value pos(kArrayType); pos.PushBack(t->position.x, a).PushBack(t->position.y, a).PushBack(t->position.z, a);
    Value rot(kArrayType); rot.PushBack(t->rotation.x, a).PushBack(t->rotation.y, a).PushBack(t->rotation.z, a).PushBack(t->rotation.w, a);
    Value scl(kArrayType); scl.PushBack(t->scale.x, a).PushBack(t->scale.y, a).PushBack(t->scale.z, a);
    tf.AddMember("position", pos, a); tf.AddMember("rotation", rot, a); tf.AddMember("scale", scl, a);
    out.AddMember("Transform", tf, a);

    Value comps(kArrayType);
    for (const auto& comp : go->getComponents()) {
        if (comp->getType() == Component::Type::Transform) continue;
        std::string data; comp->onSave(data);
        Value c(kObjectType);
        c.AddMember("Type", static_cast<int>(comp->getType()), a);
        c.AddMember("Data", Value(data.c_str(), a), a);
        comps.PushBack(c, a);
    }
    out.AddMember("Components", comps, a);

    Value children(kArrayType);
    for (const auto* child : go->getChildren()) {
        Value childNode;
        serialiseNodeInto(child, childNode, a);
        children.PushBack(childNode, a);
    }
    out.AddMember("Children", children, a);
}

GameObject* PrefabManager::deserialiseNode(const Value& node, ModuleScene* scene, GameObject* parent) {
    if (!node.IsObject()) return nullptr;
    const char* name = node.HasMember("Name") ? node["Name"].GetString() : "Unnamed";
    GameObject* go = scene->createGameObject(name, parent);
    go->setActive(node.HasMember("Active") ? node["Active"].GetBool() : true);

    if (node.HasMember("PrefabLink") && node["PrefabLink"].IsObject()) {
        const Value& lk = node["PrefabLink"];
        PrefabInstanceData linkData;
        linkData.prefabName = lk.HasMember("PrefabName") ? lk["PrefabName"].GetString() : "";
        linkData.prefabUID = lk.HasMember("PrefabUID") ? lk["PrefabUID"].GetUint() : 0;
        if (!linkData.prefabName.empty()) PrefabManager::linkInstance(go, linkData);
    }

    if (node.HasMember("Transform") && node["Transform"].IsObject()) {
        auto* t = go->getTransform();
        const Value& tf = node["Transform"];
        if (tf.HasMember("position")) { const auto& p = tf["position"]; t->position = { p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat() }; }
        if (tf.HasMember("rotation")) { const auto& r = tf["rotation"]; t->rotation = { r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat(), r[3].GetFloat() }; }
        if (tf.HasMember("scale")) { const auto& s = tf["scale"]; t->scale = { s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat() }; }
        t->markDirty();
    }

    if (node.HasMember("Components") && node["Components"].IsArray()) {
        for (SizeType i = 0; i < node["Components"].Size(); ++i)
        {
            const Value& cn = node["Components"][i];
            auto type = static_cast<Component::Type>(cn["Type"].GetInt());
            auto comp = ComponentFactory::CreateComponent(type, go);
            if (comp) { comp->onLoad(cn["Data"].GetString()); go->addComponent(std::move(comp)); }
            else LOG("PrefabManager: Could not create component type %d for '%s'", cn["Type"].GetInt(), name);
        }
    }

    if (node.HasMember("Children") && node["Children"].IsArray())
        for (SizeType i = 0; i < node["Children"].Size(); ++i) deserialiseNode(node["Children"][i], scene, go);

    return go;
}

bool PrefabManager::createPrefab(const GameObject* go, const std::string& prefabName) {
    if (!go || prefabName.empty()) { LOG("PrefabManager::createPrefab: null go or empty name"); return false; }

    app->getFileSystem()->CreateDir(kPrefabDir);
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("PrefabName", Value(prefabName.c_str(), a), a);
    doc.AddMember("Version", kPrefabFormatVersion, a);
    doc.AddMember("PrefabUID", makePrefabUID(prefabName), a);

    const PrefabInstanceData* instData = getInstanceData(go);
    if (instData && !instData->prefabName.empty() && instData->prefabName != prefabName)
        doc.AddMember("VariantOf", Value(instData->prefabName.c_str(), a), a);

    Value goNode; serialiseNodeInto(go, goNode, a);
    doc.AddMember("GameObject", goNode, a);

    std::string path = getPrefabPath(prefabName);
    if (!writePrefabDocument(doc, path)) { LOG("PrefabManager::createPrefab: Cannot open '%s' for writing", path.c_str()); return false; }
    LOG("PrefabManager: Saved prefab '%s' -> %s", prefabName.c_str(), path.c_str());
    return true;
}

GameObject* PrefabManager::instantiatePrefab(const std::string& prefabName, ModuleScene* scene) {
    if (!scene || prefabName.empty()) return nullptr;
    std::string path = getPrefabPath(prefabName);
    if (!app->getFileSystem()->Exists(path.c_str())) { LOG("PrefabManager::instantiatePrefab: Prefab not found: %s", path.c_str()); return nullptr; }

    Document doc;
    if (!readPrefabDocument(path, doc)) { LOG("PrefabManager::instantiatePrefab: JSON parse error in '%s': %s (offset %zu)", path.c_str(), GetParseError_En(doc.GetParseError()), doc.GetErrorOffset()); return nullptr; }
    if (!doc.HasMember("GameObject") || !doc["GameObject"].IsObject()) { LOG("PrefabManager::instantiatePrefab: Missing 'GameObject' in '%s'", path.c_str()); return nullptr; }

    GameObject* go = deserialiseNode(doc["GameObject"], scene, nullptr);
    if (!go) return nullptr;

    PrefabInstanceData instData;
    instData.prefabName = prefabName;
    instData.prefabUID = doc.HasMember("PrefabUID") ? doc["PrefabUID"].GetUint() : makePrefabUID(prefabName);
    linkInstance(go, instData);
    LOG("PrefabManager: Instantiated prefab '%s' -> GO '%s'", prefabName.c_str(), go->getName().c_str());
    return go;
}

bool PrefabManager::applyToPrefab(const GameObject* go, bool respectOverrides) {
    const PrefabInstanceData* inst = getInstanceData(go);
    if (!inst || inst->prefabName.empty()) { LOG("PrefabManager::applyToPrefab: '%s' is not a prefab instance", go ? go->getName().c_str() : "null"); return false; }
    if (!respectOverrides) return createPrefab(go, inst->prefabName);

    app->getFileSystem()->CreateDir(kPrefabDir);
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("PrefabName", Value(inst->prefabName.c_str(), a), a);
    doc.AddMember("Version", kPrefabFormatVersion, a);
    doc.AddMember("PrefabUID", inst->prefabUID, a);

    if (!inst->overrides.isEmpty()) {
        Value overrideMap(kObjectType);
        Value modProps(kObjectType);
        for (const auto& [compType, props] : inst->overrides.modifiedProperties) {
            Value propArr(kArrayType);
            for (const auto& p : props) propArr.PushBack(Value(p.c_str(), a), a);
            std::string key = std::to_string(compType);
            modProps.AddMember(Value(key.c_str(), a), propArr, a);
        }
        overrideMap.AddMember("ModifiedProperties", modProps, a);
        Value added(kArrayType); for (int t : inst->overrides.addedComponentTypes) added.PushBack(t, a);
        overrideMap.AddMember("AddedComponents", added, a);
        Value removed(kArrayType); for (int t : inst->overrides.removedComponentTypes) removed.PushBack(t, a);
        overrideMap.AddMember("RemovedComponents", removed, a);
        doc.AddMember("OverrideMap", overrideMap, a);
    }

    Value goNode; serialiseNodeInto(go, goNode, a);
    doc.AddMember("GameObject", goNode, a);

    std::string path = getPrefabPath(inst->prefabName);
    if (!writePrefabDocument(doc, path)) { LOG("PrefabManager::applyToPrefab: Cannot write to '%s'", path.c_str()); return false; }
    LOG("PrefabManager: Applied instance '%s' -> prefab '%s'", go->getName().c_str(), inst->prefabName.c_str());
    return true;
}

bool PrefabManager::revertToPrefab(GameObject* go, ModuleScene* scene) {
    PrefabInstanceData* inst = getInstanceDataMutable(go);
    if (!inst || inst->prefabName.empty()) { LOG("PrefabManager::revertToPrefab: '%s' is not a prefab instance", go ? go->getName().c_str() : "null"); return false; }

    std::string path = getPrefabPath(inst->prefabName);
    if (!app->getFileSystem()->Exists(path.c_str())) { LOG("PrefabManager::revertToPrefab: Prefab file missing: %s", path.c_str()); return false; }

    Document doc;
    if (!readPrefabDocument(path, doc) || !doc.HasMember("GameObject")) return false;

    const Value& node = doc["GameObject"];
    PrefabOverrideRecord savedOverrides = inst->overrides;

    const int kTransformType = static_cast<int>(Component::Type::Transform);
    auto* t = go->getTransform();
    if (node.HasMember("Transform") && node["Transform"].IsObject()) {
        const Value& tf = node["Transform"];
        auto ovIt = savedOverrides.modifiedProperties.find(kTransformType);
        const std::unordered_set<std::string>* ovSet = (ovIt != savedOverrides.modifiedProperties.end()) ? &ovIt->second : nullptr;
        auto overridden = [&](const char* prop) { return ovSet && ovSet->count(prop) > 0; };

        if (!overridden("position") && tf.HasMember("position")) { const auto& p = tf["position"]; t->position = { p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat() }; }
        if (!overridden("rotation") && tf.HasMember("rotation")) { const auto& r = tf["rotation"]; t->rotation = { r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat(), r[3].GetFloat() }; }
        if (!overridden("scale") && tf.HasMember("scale")) { const auto& s = tf["scale"]; t->scale = { s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat() }; }
        t->markDirty();
    }

    if (node.HasMember("Components") && node["Components"].IsArray()) {
        for (SizeType i = 0; i < node["Components"].Size(); ++i) {
            const Value& cn = node["Components"][i];
            int typeInt = cn["Type"].GetInt();
            auto type = static_cast<Component::Type>(typeInt);
            const auto& removedVec = savedOverrides.removedComponentTypes;
            if (std::find(removedVec.begin(), removedVec.end(), typeInt) != removedVec.end()) continue;

            Component* existing = nullptr;
            for (const auto& c : go->getComponents()) if (c->getType() == type) { existing = c.get(); break; }

            if (!existing) {
                auto comp = ComponentFactory::CreateComponent(type, go);
                if (comp) { comp->onLoad(cn["Data"].GetString()); go->addComponent(std::move(comp)); }
            }
            else {
                auto it = savedOverrides.modifiedProperties.find(typeInt);
                bool fullyOverridden = (it != savedOverrides.modifiedProperties.end() && it->second.count("__all__") > 0);
                if (!fullyOverridden) existing->onLoad(cn["Data"].GetString());
            }
        }
    }

    inst->overrides = savedOverrides;
    LOG("PrefabManager: Reverted '%s' from prefab '%s'", go->getName().c_str(), inst->prefabName.c_str());
    return true;
}

void PrefabManager::markPropertyOverride(GameObject* go, int componentType, const std::string& propertyName) {
    PrefabInstanceData* inst = getInstanceDataMutable(go);
    if (inst) inst->overrides.modifiedProperties[componentType].insert(propertyName);
}

void PrefabManager::clearComponentOverrides(GameObject* go, int componentType) {
    PrefabInstanceData* inst = getInstanceDataMutable(go);
    if (!inst) return;
    inst->overrides.modifiedProperties.erase(componentType);
    auto& av = inst->overrides.addedComponentTypes; av.erase(std::remove(av.begin(), av.end(), componentType), av.end());
    auto& rv = inst->overrides.removedComponentTypes; rv.erase(std::remove(rv.begin(), rv.end(), componentType), rv.end());
}

void PrefabManager::clearAllOverrides(GameObject* go) {
    PrefabInstanceData* inst = getInstanceDataMutable(go);
    if (inst) inst->overrides.clear();
}

std::string PrefabManager::serializeGameObject(const GameObject* go) {
    if (!go) return {};
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    Value goNode; serialiseNodeInto(go, goNode, a);
    doc.AddMember("GameObject", goNode, a);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    doc.Accept(writer);
    return sb.GetString();
}

GameObject* PrefabManager::deserializeGameObject(const std::string& data, ModuleScene* scene) {
    if (data.empty() || !scene) return nullptr;
    Document doc;
    doc.Parse(data.c_str());
    if (doc.HasParseError() || !doc.HasMember("GameObject") || !doc["GameObject"].IsObject()) return nullptr;
    return deserialiseNode(doc["GameObject"], scene, nullptr);
}

bool PrefabManager::createVariant(const std::string& srcPrefabName, const std::string& dstPrefabName) {
    if (srcPrefabName.empty() || dstPrefabName.empty()) return false;
    std::string srcPath = getPrefabPath(srcPrefabName);
    if (!app->getFileSystem()->Exists(srcPath.c_str())) { LOG("PrefabManager::createVariant: Source not found: %s", srcPath.c_str()); return false; }

    app->getFileSystem()->CreateDir(kPrefabDir);
    Document doc;
    if (!readPrefabDocument(srcPath, doc)) return false;

    auto& a = doc.GetAllocator();
    auto setOrAdd = [&](const char* key, const std::string& val) {
        if (doc.HasMember(key)) doc[key].SetString(val.c_str(), a);
        else doc.AddMember(Value(key, a), Value(val.c_str(), a), a);
        };
    setOrAdd("PrefabName", dstPrefabName);
    setOrAdd("VariantOf", srcPrefabName);

    if (doc.HasMember("PrefabUID")) doc["PrefabUID"].SetUint(makePrefabUID(dstPrefabName));
    else doc.AddMember("PrefabUID", makePrefabUID(dstPrefabName), a);

    if (!writePrefabDocument(doc, getPrefabPath(dstPrefabName))) return false;
    LOG("PrefabManager: Created variant '%s' from '%s'", dstPrefabName.c_str(), srcPrefabName.c_str());
    return true;
}

bool PrefabManager::isPrefabInstance(const GameObject* go) { return go && registry().count(go) > 0; }
std::string PrefabManager::getPrefabName(const GameObject* go) { const PrefabInstanceData* d = getInstanceData(go); return d ? d->prefabName : ""; }
uint32_t PrefabManager::getPrefabUID(const GameObject* go) { const PrefabInstanceData* d = getInstanceData(go); return d ? d->prefabUID : 0; }

const PrefabInstanceData* PrefabManager::getInstanceData(const GameObject* go) {
    auto it = registry().find(go);
    return (it != registry().end()) ? &it->second : nullptr;
}

PrefabInstanceData* PrefabManager::getInstanceDataMutable(GameObject* go) {
    auto it = registry().find(go);
    return (it != registry().end()) ? &it->second : nullptr;
}

std::vector<PrefabManager::PrefabInfo> PrefabManager::listPrefabsInfo() {
    std::vector<PrefabInfo> results;
    if (!app->getFileSystem()->Exists(kPrefabDir)) return results;

    try {
        for (const auto& entry : fs::directory_iterator(kPrefabDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != kPrefabExt) continue;
            PrefabInfo info;
            info.name = entry.path().stem().string();
            Document doc;
            if (!readPrefabDocument(entry.path().string(), doc)) continue;

            info.uid = doc.HasMember("PrefabUID") ? doc["PrefabUID"].GetUint() : 0;
            info.version = doc.HasMember("Version") ? doc["Version"].GetInt() : 0;
            if (doc.HasMember("VariantOf") && doc["VariantOf"].IsString()) { info.variantOf = doc["VariantOf"].GetString(); info.isVariant = true; }

            if (doc.HasMember("GameObject") && doc["GameObject"].IsObject()) {
                const Value& gn = doc["GameObject"];
                std::vector<std::string> compNames = { "Transform" };
                if (gn.HasMember("Components") && gn["Components"].IsArray()) {
                    for (SizeType i = 0; i < gn["Components"].Size(); ++i) {
                        switch (static_cast<Component::Type>(gn["Components"][i]["Type"].GetInt()))
                        {
                        case Component::Type::Mesh: compNames.push_back("Mesh"); break;
                        case Component::Type::Camera: compNames.push_back("Camera"); break;
                        case Component::Type::DirectionalLight: compNames.push_back("DirLight"); break;
                        case Component::Type::PointLight: compNames.push_back("PointLight"); break;
                        case Component::Type::SpotLight: compNames.push_back("SpotLight"); break;
                        default: compNames.push_back("Unknown"); break;
                        }
                    }
                }
                for (size_t i = 0; i < compNames.size(); ++i) { if (i) info.componentSummary += ", "; info.componentSummary += compNames[i]; }

                std::function<int(const Value&)> countChildren = [&](const Value& n) -> int
                    {
                        int c = 0;
                        if (n.HasMember("Children") && n["Children"].IsArray()) {
                            c += static_cast<int>(n["Children"].Size());
                            for (SizeType i = 0; i < n["Children"].Size(); ++i) c += countChildren(n["Children"][i]);
                        }
                        return c;
                    };
                info.childCount = countChildren(gn);
            }
            results.push_back(std::move(info));
        }
    }
    catch (...) {}
    return results;
}

std::vector<std::string> PrefabManager::listPrefabs() {
    std::vector<std::string> names;
    if (!app->getFileSystem()->Exists(kPrefabDir)) return names;
    try { for (const auto& e : fs::directory_iterator(kPrefabDir)) if (e.is_regular_file() && e.path().extension() == kPrefabExt) names.push_back(e.path().stem().string()); }
    catch (...) {}
    return names;
}

bool PrefabManager::prefabExists(const std::string& prefabName) { return app->getFileSystem()->Exists(getPrefabPath(prefabName).c_str()); }
void PrefabManager::linkInstance(GameObject* go, const PrefabInstanceData& data) { if (go) registry()[go] = data; }
void PrefabManager::unlinkInstance(GameObject* go) { if (go) registry().erase(go); }