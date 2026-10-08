#include "Globals.h"
#include "AshfallHubBuilder.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ResourceModel.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "HotReloadManager.h"
#include "PrefabManager.h"
#include "ComponentFactory.h"
#include "ComponentScript.h"
#include "ComponentAnimation.h"
#include "ComponentMesh.h"
#include "ComponentTransform2D.h"
#include "ComponentCanvas.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentButton.h"
#include "3rdParty/rapidjson/document.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <vector>

using namespace rapidjson;

namespace {
    constexpr const char* kPack = "Assets/Ashfall_UI/";

    bool readJson(const std::string& path, Document& doc){
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        doc.Parse(text.c_str());
        return !doc.HasParseError() && doc.IsObject();
    }

    bool scriptLoaded(HotReloadManager* hotReload, const char* className){
        const std::vector<std::string> classes = hotReload ? hotReload->getRegisteredClassNames() : std::vector<std::string>();
        return std::find(classes.begin(), classes.end(), className) != classes.end();
    }

    void destroySubtree(SceneGraph* scene, GameObject* root){
        // destroyGameObject only reparents a node's children, so collect the subtree and remove it bottom-up.
        std::vector<GameObject*> subtree;
        std::function<void(GameObject*)> collect = [&](GameObject* n){
            subtree.push_back(n);
            for (GameObject* c : n->getChildren()) collect(c);
        };
        collect(root);
        for (int i = (int)subtree.size() - 1; i >= 0; --i) scene->destroyGameObject(subtree[i]);
    }

    void addScript(GameObject* go, const std::string& json){
        auto comp = ComponentFactory::CreateComponent(Component::Type::Script, go);
        comp->onLoad(json);   // ClassName (connects the live script instance) + Fields
        go->addComponent(std::move(comp));
    }

    struct NpcDef {
        const char* prefab;
        const char* npcId;          // key into hub_content.json -> npcs
        const char* model;          // under Assets/
        const char* stateMachine;   // under Assets/
        // Replaces the glTF's own Armature rotation (+90 deg X in all of these) when set. Peasant's clips key the
        // `bones` node back to its lying rest pose, so with the usual +90 X Mira idles face-down; identity stands
        // her up.
        std::optional<Quaternion> armatureRotation = std::nullopt;
    };
    // The Characters page mapping (see docs/HUB.md).
    const NpcDef kNpcs[] = {
        { "NPC_Bruck",   "Bruck",   "Character Models/Dwarf/GLTF Model/Dwarf.gltf",            "StateMachines/Dwarf.json" },
        { "NPC_Mira",    "Mira",    "Character Models/Peasant/GLTF Model/Peasant.gltf",        "StateMachines/Peasant.json", Quaternion::Identity },
        { "NPC_Raniver", "Raniver", "Character Models/Elf/GLTF Model/Elf_arbalester.gltf",     "StateMachines/Elf_arbalester.json" },
        { "NPC_Oskar",   "Oskar",   "Character Models/Mage/GLTF Model/Mage.gltf",              "StateMachines/Mage.json" },
    };

    // Same path the Asset Browser hands ModuleEditor::spawnModel; the "Assets/..." form is a fallback.
    UID modelUID(const std::string& assetsRoot, const std::string& relative){
        UID uid = app->getAssets()->findUID(assetsRoot + relative);
        return uid ? uid : app->getAssets()->findUID(std::string("Assets/") + relative);
    }

    void tagMeshNodes(GameObject* node, const std::string& tag){
        if (node->getComponent<ComponentMesh>()) node->setTag(tag);
        for (GameObject* c : node->getChildren()) tagMeshNodes(c, tag);
    }

    struct Ctx {
        SceneGraph* scene;
        const Value* colors;   // hub_pages_layout.json "colors"
        const Value* tokens;   // ui_tokens.json "colors"
        int missingTextures = 0;
        std::string assetsRoot;
        std::string firstMissing;
    };

    const Value* member(const Value* obj, const char* key){
        return (obj && obj->IsObject() && obj->HasMember(key)) ? &(*obj)[key] : nullptr;
    }
    float num(const Value& obj, const char* key, float fallback){
        return (obj.HasMember(key) && obj[key].IsNumber()) ? obj[key].GetFloat() : fallback;
    }
    std::string str(const Value& obj, const char* key, const std::string& fallback = ""){
        return (obj.HasMember(key) && obj[key].IsString()) ? obj[key].GetString() : fallback;
    }
    Vector2 vec2(const Value& obj, const char* key, Vector2 fallback){
        if (!obj.HasMember(key) || !obj[key].IsArray() || obj[key].Size() < 2) return fallback;
        return Vector2(obj[key][0].GetFloat(), obj[key][1].GetFloat());
    }

    // "#RRGGBB", "#RRGGBBAA", or the name of a colour in the layout's "colors" or in ui_tokens.json.
    Vector4 parseColor(const Ctx& ctx, const std::string& value, Vector4 fallback){
        std::string hex = value;
        for (int pass = 0; pass < 3 && !hex.empty() && hex[0] != '#'; ++pass){
            const Value* named = member(ctx.colors, hex.c_str());
            if (!named) named = member(ctx.tokens, hex.c_str());
            if (!named || !named->IsString()) return fallback;
            hex = named->GetString();
        }
        if (hex.size() != 7 && hex.size() != 9) return fallback;
        const unsigned long v = std::strtoul(hex.c_str() + 1, nullptr, 16);
        if (hex.size() == 7)
            return Vector4(((v >> 16) & 0xFF) / 255.f, ((v >> 8) & 0xFF) / 255.f, (v & 0xFF) / 255.f, 1.f);
        return Vector4(((v >> 24) & 0xFF) / 255.f, ((v >> 16) & 0xFF) / 255.f, ((v >> 8) & 0xFF) / 255.f, (v & 0xFF) / 255.f);
    }
    Vector4 colorOf(const Ctx& ctx, const Value& obj, const char* key, Vector4 fallback){
        Vector4 c = obj.HasMember(key) && obj[key].IsString() ? parseColor(ctx, obj[key].GetString(), fallback) : fallback;
        if (obj.HasMember("alpha") && obj["alpha"].IsNumber()) c.w = obj["alpha"].GetFloat();
        return c;
    }

    std::string texturePath(Ctx& ctx, const std::string& relative){
        if (relative.empty()) return {};
        const std::string full = std::string(kPack) + relative;
        std::ifstream probe(ctx.assetsRoot + "Ashfall_UI/" + relative, std::ios::binary);
        if (!probe){
            if (!ctx.missingTextures++) ctx.firstMissing = full;
            PHX_LOG(Editor, Warning, "[Hub] missing texture %s", full.c_str());
        }
        return full;
    }

    std::string substitute(std::string s, int index){
        if (index < 0) return s;
        for (size_t p; (p = s.find("{i}")) != std::string::npos; ) s.replace(p, 3, std::to_string(index));
        return s;
    }

    template<typename T>
    T* add(GameObject* go, Component::Type type){
        go->addComponent(ComponentFactory::CreateComponent(type, go));
        return go->getComponent<T>();
    }

    // Builds one layout node (and, for "repeat", its copies) under `parent`. See hub_pages_layout.json "_schema".
    void buildNode(Ctx& ctx, GameObject* parent, const Value& n, int index, Vector2 offset);

    void buildOne(Ctx& ctx, GameObject* parent, const Value& n, int index, Vector2 offset){
        GameObject* go = ctx.scene->createGameObject(substitute(str(n, "name", "HUB_Node"), index), parent);
        auto* t = add<ComponentTransform2D>(go, Component::Type::Transform2D);
        if (n.HasMember("stretch")){
            const float px = num(n, "stretch", 0.f);
            t->anchorMin = Vector2(0.f, 0.f); t->anchorMax = Vector2(1.f, 1.f); t->pivot = Vector2(.5f, .5f);
            t->position = Vector2::Zero; t->size = Vector2(-2.f * px, -2.f * px);
        }
        else if (n.HasMember("pin") && n["pin"].IsObject()){
            const Value& p = n["pin"];
            const Vector2 anchor = vec2(p, "anchor", Vector2(.5f, .5f));
            t->anchorMin = t->anchorMax = anchor;
            t->pivot = vec2(p, "pivot", anchor);
            t->position = vec2(p, "pos", Vector2::Zero) + offset;
            t->size = vec2(p, "size", Vector2(100.f, 100.f));
        }
        else {
            // "rect": [x, y, w, h] = top-left corner inside the parent, fixed size.
            float r[4] = { 0.f, 0.f, 100.f, 100.f };
            if (n.HasMember("rect") && n["rect"].IsArray())
                for (SizeType i = 0; i < 4 && i < n["rect"].Size(); ++i) r[i] = n["rect"][i].GetFloat();
            t->anchorMin = t->anchorMax = t->pivot = Vector2(0.f, 0.f);
            t->position = Vector2(r[0], r[1]) + offset;
            t->size = Vector2(r[2], r[3]);
        }
        t->visible = !(n.HasMember("hidden") && n["hidden"].IsBool() && n["hidden"].GetBool());

        const bool isButton = n.HasMember("button") && n["button"].IsObject();
        if (n.HasMember("image")){
            auto* img = add<ComponentImage>(go, Component::Type::Image);
            img->texturePath = texturePath(ctx, n["image"].IsString() ? n["image"].GetString() : "");
            img->tint = colorOf(ctx, n, "tint", Vector4(1.f, 1.f, 1.f, 1.f));
            img->raycastTarget = n.HasMember("raycast") && n["raycast"].IsBool() ? n["raycast"].GetBool() : isButton;
        }
        if (isButton){
            const Value& b = n["button"];
            auto* btn = add<ComponentButton>(go, Component::Type::Button);
            btn->navigable = false;   // Space / Enter would press a Tab-focused button; the pages have their own keys
            btn->hoverTexture = texturePath(ctx, str(b, "hover"));
            btn->pressedTexture = texturePath(ctx, str(b, "pressed"));
            btn->disabledTexture = texturePath(ctx, str(b, "disabled"));
            // A sprite swap carries the state on its own; keep the colour multipliers neutral for those states.
            if (!btn->hoverTexture.empty()) btn->hoverColor = Vector4(1.f, 1.f, 1.f, 1.f);
            if (!btn->pressedTexture.empty()) btn->pressedColor = Vector4(1.f, 1.f, 1.f, 1.f);
            if (!btn->disabledTexture.empty()) btn->disabledColor = Vector4(1.f, 1.f, 1.f, 1.f);
        }
        if (n.HasMember("label") && n["label"].IsObject()){
            const Value& l = n["label"];
            auto* c = add<ComponentLabel>(go, Component::Type::Label);
            c->text = substitute(str(l, "text"), index);
            c->fontName = str(l, "font", "AshfallUI");
            c->fontSize = num(l, "size", 18.f);
            c->color = colorOf(ctx, l, "color", Vector4(1.f, 1.f, 1.f, 1.f));
            const std::string h = str(l, "align", "left"), v = str(l, "valign", "middle");
            c->hAlign = h == "center" ? ComponentLabel::HAlign::Center : h == "right" ? ComponentLabel::HAlign::Right : ComponentLabel::HAlign::Left;
            c->vAlign = v == "top" ? ComponentLabel::VAlign::Top : v == "bottom" ? ComponentLabel::VAlign::Bottom : ComponentLabel::VAlign::Middle;
        }

        if (n.HasMember("children") && n["children"].IsArray())
            for (const Value& child : n["children"].GetArray()) buildNode(ctx, go, child, index, Vector2::Zero);
    }

    void buildNode(Ctx& ctx, GameObject* parent, const Value& n, int index, Vector2 offset){
        if (!n.IsObject()) return;
        const Value* rep = member(&n, "repeat");
        if (!rep){ buildOne(ctx, parent, n, index, offset); return; }
        // Copies {i} = start .. start+count-1, each moved by `step` from the previous one.
        const int count = (int)num(*rep, "count", 1.f);
        const int start = (int)num(*rep, "start", 0.f);
        const Vector2 step = vec2(*rep, "step", Vector2::Zero);
        for (int i = 0; i < count; ++i) buildOne(ctx, parent, n, start + i, offset + step * (float)i);
    }
}

bool BuildAshfallHubNPCPrefabs(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage){
    if (!scene){ outMessage = "no active scene"; return false; }
    if (!scriptLoaded(hotReload, "HubNPC")){
        outMessage = "script class 'HubNPC' is not loaded: build GameScript.dll first";
        return false;
    }

    const std::string assets = app->getFileSystem()->GetAssetsPath();
    std::string saved;
    for (const NpcDef& def : kNpcs){
        const UID uid = modelUID(assets, def.model);
        ResourceModel* model = uid ? app->getResources()->RequestModel(uid) : nullptr;
        if (!model){
            outMessage = std::string("can't load ") + def.model + " (is it imported?)" + (saved.empty() ? "" : "; saved so far: " + saved);
            return false;
        }
        GameObject* root = model->spawnIntoScene(scene);
        app->getResources()->ReleaseResource(model);
        if (!root){ outMessage = std::string("spawning ") + def.model + " failed"; return false; }

        root->setName(def.prefab);
        root->setTag("");
        if (def.armatureRotation){
            bool found = false;
            for (GameObject* c : root->getChildren())
                if (c->getName() == "Armature"){
                    c->getTransform()->rotation = *def.armatureRotation;
                    c->getTransform()->markDirty();
                    found = true;
                }
            if (!found) PHX_LOG(Editor, Warning, "[Hub] %s: no Armature child to apply the rotation override to", def.prefab);
        }
        if (auto* anim = root->getComponent<ComponentAnimation>())
            anim->LoadStateMachineFromPath(assets + def.stateMachine);   // starts its DefaultState (idle)
        else
            PHX_LOG(Editor, Warning, "[Hub] %s has no ComponentAnimation: delete Library/Animations/<model>/ and re-import", def.model);
        // The spring arm passes through the NPC; Sera still collides with it (collision only reads the top-level tag).
        for (GameObject* c : root->getChildren()) tagMeshNodes(c, "CameraIgnore");

        addScript(root, std::string("{\"ClassName\":\"HubNPC\",\"Fields\":{\"NpcId\":\"") + def.npcId + "\"}}");

        if (!PrefabManager::createPrefab(root, def.prefab)){
            outMessage = std::string("could not write the ") + def.prefab + " prefab: it is left in the scene";
            return false;
        }
        destroySubtree(scene, root);
        saved += (saved.empty() ? "" : ", ") + std::string(def.prefab);
    }
    outMessage = saved + " in " + app->getFileSystem()->GetLibraryPath() + "Prefabs/";
    return true;
}

bool BuildAshfallHubPagesPrefab(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage){
    if (!scene){ outMessage = "no active scene"; return false; }

    const std::string assets = app->getFileSystem()->GetAssetsPath();
    Document layout, tokens;
    if (!readJson(assets + "Ashfall_UI/hub_pages_layout.json", layout)){
        outMessage = assets + "Ashfall_UI/hub_pages_layout.json is missing or not valid JSON";
        return false;
    }
    readJson(assets + "Ashfall_UI/ui_tokens.json", tokens);

    const std::string scriptClass = str(layout, "script", "HubPages");
    if (!scriptLoaded(hotReload, scriptClass.c_str())){
        outMessage = "script class '" + scriptClass + "' is not loaded: build GameScript.dll first";
        return false;
    }
    const Value* rootDef = member(&layout, "root");
    if (!rootDef || !rootDef->IsObject()){ outMessage = "hub_pages_layout.json has no \"root\""; return false; }

    Ctx ctx{ scene, member(&layout, "colors"), tokens.IsObject() ? member(&tokens, "colors") : nullptr };
    ctx.assetsRoot = assets;

    GameObject* root = scene->createGameObject(str(*rootDef, "name", "HUB_Root"));
    auto* canvas = add<ComponentCanvas>(root, Component::Type::Canvas);
    canvas->sortOrder = (int)num(layout, "sortOrder", 20.f);
    canvas->scaleMode = ComponentCanvas::ScaleMode::ScaleWithScreenSize;
    canvas->referenceResolution = vec2(layout, "reference", Vector2(1920.f, 1080.f));
    canvas->matchWidthOrHeight = 0.5f;
    addScript(root, "{\"ClassName\":\"" + scriptClass + "\"}");

    if (rootDef->HasMember("children") && (*rootDef)["children"].IsArray())
        for (const Value& child : (*rootDef)["children"].GetArray()) buildNode(ctx, root, child, -1, Vector2::Zero);

    const std::string prefab = str(layout, "prefab", kAshfallHubPagesPrefab);
    if (!PrefabManager::createPrefab(root, prefab)){
        outMessage = "could not write the " + prefab + " prefab: the built hierarchy is left in the scene";
        return false;
    }
    destroySubtree(scene, root);
    outMessage = app->getFileSystem()->GetLibraryPath() + "Prefabs/" + prefab + ".prefab";
    if (ctx.missingTextures)
        outMessage += " (WARNING: " + std::to_string(ctx.missingTextures) + " missing texture(s), first " + ctx.firstMissing + ")";
    return true;
}
