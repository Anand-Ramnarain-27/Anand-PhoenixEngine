#include "Globals.h"
#include "AshfallHUDBuilder.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "HotReloadManager.h"
#include "PrefabManager.h"
#include "ComponentFactory.h"
#include "ComponentScript.h"
#include "ComponentTransform2D.h"
#include "ComponentCanvas.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentProgressBar.h"
#include "3rdParty/rapidjson/document.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <vector>

using namespace rapidjson;

namespace {
    constexpr const char* kScriptClass = "AshfallHUD";
    constexpr const char* kPack = "Assets/Ashfall_UI/";

    std::string tex(const char* relative){ return std::string(kPack) + relative; }

    // ---- data files ----
    bool readJson(const std::string& path, Document& doc){
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        doc.Parse(text.c_str());
        return !doc.HasParseError() && doc.IsObject();
    }

    const Value* member(const Value* obj, const char* key){
        return (obj && obj->IsObject() && obj->HasMember(key)) ? &(*obj)[key] : nullptr;
    }
    float num(const Value* obj, const char* key, float fallback){
        const Value* v = member(obj, key);
        return (v && v->IsNumber()) ? v->GetFloat() : fallback;
    }
    Vector2 vec2(const Value* obj, const char* key, Vector2 fallback){
        const Value* v = member(obj, key);
        if (!v || !v->IsArray() || v->Size() < 2 || !(*v)[0].IsNumber() || !(*v)[1].IsNumber()) return fallback;
        return Vector2((*v)[0].GetFloat(), (*v)[1].GetFloat());
    }
    // [top, right, bottom, left], as in the design files.
    Vector4 edges(const Value* obj, const char* key, Vector4 fallback){
        const Value* v = member(obj, key);
        if (!v || !v->IsArray() || v->Size() < 4) return fallback;
        for (SizeType i = 0; i < 4; ++i) if (!(*v)[i].IsNumber()) return fallback;
        return Vector4((*v)[0].GetFloat(), (*v)[1].GetFloat(), (*v)[2].GetFloat(), (*v)[3].GetFloat());
    }
    Vector4 color(const Value* colors, const char* key, const char* fallbackHex){
        const Value* v = member(colors, key);
        const char* hex = (v && v->IsString()) ? v->GetString() : fallbackHex;
        if (*hex == '#') ++hex;
        const unsigned rgb = (unsigned)std::strtoul(hex, nullptr, 16);
        return Vector4(((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, 1.f);
    }

    // ---- layout ----
    struct Layout {
        Vector2 anchorMin, anchorMax, pivot, position, size;
    };
    // Top-left corner at `pos` inside the parent, fixed size.
    Layout at(float x, float y, float w, float h){ return { { 0.f, 0.f }, { 0.f, 0.f }, { 0.f, 0.f }, { x, y }, { w, h } }; }
    // Anchored to a point of the parent with the pivot on the same point, so `pos` is the offset from it.
    Layout pinned(Vector2 anchor, Vector2 pos, Vector2 size){ return { anchor, anchor, anchor, pos, size }; }
    // Fills the parent, shrunk by `px` on every side.
    Layout inset(float px){ return { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, Vector2::Zero, { -2.f * px, -2.f * px } }; }

    struct Builder {
        SceneGraph* scene;

        template<typename T>
        T* add(GameObject* go, Component::Type type){
            go->addComponent(ComponentFactory::CreateComponent(type, go));
            return go->getComponent<T>();
        }

        GameObject* node(GameObject* parent, const std::string& name, const Layout& l, bool visible = true){
            GameObject* go = scene->createGameObject(name, parent);
            auto* t = add<ComponentTransform2D>(go, Component::Type::Transform2D);
            t->anchorMin = l.anchorMin;
            t->anchorMax = l.anchorMax;
            t->pivot = l.pivot;
            t->position = l.position;
            t->size = l.size;
            t->visible = visible;
            return go;
        }

        // HUD images never take the pointer: the game, not the HUD, owns clicks.
        GameObject* image(GameObject* parent, const std::string& name, const Layout& l, const std::string& texture,
                          Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f), bool visible = true){
            GameObject* go = node(parent, name, l, visible);
            auto* img = add<ComponentImage>(go, Component::Type::Image);
            img->texturePath = texture;
            img->tint = tint;
            img->raycastTarget = false;
            return go;
        }

        // Fill-only bar: no background quad and an untinted fill texture (the component defaults to a dark
        // background and a green fill tint).
        GameObject* bar(GameObject* parent, const std::string& name, const Layout& l, const std::string& fillTexture,
                        ComponentProgressBar::FillDirection direction, float value, bool visible = true){
            GameObject* go = node(parent, name, l, visible);
            auto* b = add<ComponentProgressBar>(go, Component::Type::ProgressBar);
            b->direction = direction;
            b->value = value;
            b->backgroundColor = Vector4(0.f, 0.f, 0.f, 0.f);
            b->fillColor = Vector4(1.f, 1.f, 1.f, 1.f);
            b->fillTexture = fillTexture;
            return go;
        }

        GameObject* label(GameObject* parent, const std::string& name, const Layout& l, const std::string& text,
                          const char* font, float size, Vector4 color,
                          ComponentLabel::HAlign h = ComponentLabel::HAlign::Left,
                          ComponentLabel::VAlign v = ComponentLabel::VAlign::Middle, bool visible = true){
            GameObject* go = node(parent, name, l, visible);
            auto* c = add<ComponentLabel>(go, Component::Type::Label);
            c->text = text;
            c->fontName = font;
            c->fontSize = size;
            c->color = color;
            c->hAlign = h;
            c->vAlign = v;
            return go;
        }
    };

    struct SlotDef {
        const char* id;
        const char* icon;
        bool signature;
    };
    // Group separators ("|" in hud_layout.json's abilityBar.order) are the null ids.
    const SlotDef kSlots[] = {
        { "Light", "icon_sword", false }, { "Heavy", "icon_heavy", false }, { nullptr, nullptr, false },
        { "Dodge", "icon_dodge", false }, { "Block", "icon_shield", false }, { nullptr, nullptr, false },
        { "A1", "icon_bash", false }, { "A2", "icon_smite", false }, { "A3", "icon_aegis", false }, { "A4", "icon_rally", false },
        { nullptr, nullptr, false },
        { "Signature", "icon_vow", true },
    };

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
}

bool BuildAshfallHUDPrefab(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage){
    if (!scene){ outMessage = "no active scene"; return false; }

    const std::vector<std::string> classes = hotReload ? hotReload->getRegisteredClassNames() : std::vector<std::string>();
    if (std::find(classes.begin(), classes.end(), kScriptClass) == classes.end()){
        outMessage = std::string("script class '") + kScriptClass + "' is not loaded: build GameScript.dll first";
        return false;
    }

    const std::string assets = app->getFileSystem()->GetAssetsPath() + "Ashfall_UI/";
    Document layoutDoc, tokensDoc;
    if (!readJson(assets + "hud_layout.json", layoutDoc))
        LOG("[HUD] %shud_layout.json missing or invalid: using built-in 1080p defaults", assets.c_str());
    if (!readJson(assets + "ui_tokens.json", tokensDoc))
        LOG("[HUD] %sui_tokens.json missing or invalid: using built-in colours", assets.c_str());

    const Value* layout = layoutDoc.IsObject() ? &layoutDoc : nullptr;
    const Value* colors = tokensDoc.IsObject() ? member(&tokensDoc, "colors") : nullptr;
    const Vector4 parchment = color(colors, "parchment", "#EDE3CF");
    const Vector4 ash = color(colors, "ash", "#A2968A");
    const Vector4 brass = color(colors, "brass", "#B89A62");
    const Vector4 brassLight = color(colors, "brassLight", "#DCC392");
    const Vector4 white(1.f, 1.f, 1.f, 1.f);

    using H = ComponentLabel::HAlign;
    using V = ComponentLabel::VAlign;
    using Dir = ComponentProgressBar::FillDirection;
    Builder b{ scene };

    // ---- root ----
    GameObject* root = scene->createGameObject("HUD_Root");
    auto* canvas = b.add<ComponentCanvas>(root, Component::Type::Canvas);
    canvas->sortOrder = 10;
    canvas->scaleMode = ComponentCanvas::ScaleMode::ScaleWithScreenSize;
    canvas->referenceResolution = vec2(layout, "reference", Vector2(1920.f, 1080.f));
    canvas->matchWidthOrHeight = 0.5f;
    {
        auto comp = ComponentFactory::CreateComponent(Component::Type::Script, root);
        static_cast<ComponentScript*>(comp.get())->setScriptClass(kScriptClass, hotReload);
        root->addComponent(std::move(comp));
    }

    // A canvas root is never drawn itself, so hiding "the whole HUD" means hiding this stretched child.
    GameObject* main = b.node(root, "HUD_Main", inset(0.f));

    b.image(main, "HUD_Vignette", inset(0.f), tex("HUD/Screen/vignette_dark.png"));
    b.image(main, "HUD_LowHP", inset(0.f), tex("HUD/Screen/vignette_lowhp.png"), white, /*visible=*/false);

    // ---- player frame (top-left) ----
    {
        const Value* pf = member(layout, "playerFrame");
        const Vector2 offset = vec2(pf, "offset", Vector2(40.f, 36.f));
        const float medallion = num(pf, "medallion", 120.f);
        const float gap = num(pf, "gap", 16.f);
        const Vector2 hpBar = vec2(pf, "hpBar", Vector2(440.f, 24.f));
        const float nameSize = num(member(pf, "name"), "size", 28.f);
        const float statusSize = num(member(pf, "statusRow"), "size", 30.f);
        const float statusGap = num(member(pf, "statusRow"), "gap", 8.f);
        const float textX = medallion + gap;

        GameObject* player = b.node(main, "HUD_Player", pinned({ 0.f, 0.f }, offset, { textX + hpBar.x, medallion + 8.f }));

        GameObject* med = b.node(player, "HUD_Medallion", at(0.f, 0.f, medallion, medallion));
        b.image(med, "HUD_VowTrack", inset(0.f), tex("HUD/Portrait/medallion_track_120.png"));
        b.image(med, "HUD_VowRing", inset(0.f), tex("HUD/Portrait/VowFill/vow_ring_fill_000_120.png"), Vector4(1.f, 1.f, 1.f, .55f));
        // vow_ring_glow_120 is 160x160 with 20 px of glow on every side.
        b.image(med, "HUD_VowGlow", at(-20.f, -20.f, medallion + 40.f, medallion + 40.f), tex("HUD/Portrait/vow_ring_glow_120.png"), white, false);
        const float portrait = 94.f, portraitAt = (medallion - portrait) * 0.5f;
        b.image(med, "HUD_PortraitBg", at(portraitAt, portraitAt, portrait, portrait), tex("HUD/Portrait/portrait_bg_94.png"));
        b.image(med, "HUD_Portrait", at(portraitAt, portraitAt, portrait, portrait), tex("HUD/Portrait/monogram_S_94.png"));   // placeholder art
        b.image(med, "HUD_PortraitRim", at(portraitAt, portraitAt, portrait, portrait), tex("HUD/Portrait/portrait_rim_brass_94.png"));

        b.label(player, "HUD_Name", at(textX, 0.f, hpBar.x, nameSize + 6.f), "Sera Vantry", "AshfallDisplay", nameSize, parchment);
        b.label(player, "HUD_Class", at(textX, 0.f, hpBar.x, nameSize + 6.f), "PALADIN", "AshfallLabel", 15.f, ash, H::Right);

        // The frame's 2 px border stays visible: the bars sit inside it.
        GameObject* hp = b.image(player, "HUD_HPFrame", at(textX, 40.f, hpBar.x, hpBar.y), tex("HUD/Sized/hp_frame_440x24.png"));
        b.bar(hp, "HUD_HPTrail", inset(2.f), tex("HUD/Bars/bar_fill_ember_trail.png"), Dir::LeftToRight, 1.f);
        b.bar(hp, "HUD_HP", inset(2.f), tex("HUD/Bars/bar_fill_blood.png"), Dir::LeftToRight, 1.f);
        b.bar(hp, "HUD_HPLow", inset(2.f), tex("HUD/Bars/bar_fill_blood_low.png"), Dir::LeftToRight, 1.f, false);
        b.label(hp, "HUD_HPText", { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, { 0.f, 0.f }, { -16.f, 0.f } },
                "100 / 100", "AshfallUI", 15.f, parchment, H::Right);

        static const char* kStatus[] = { "status_aegis", "status_marked", "status_snare", "status_stagger" };
        for (int i = 0; i < 4; ++i)
            b.image(player, "HUD_Status" + std::to_string(i), at(textX + i * (statusSize + statusGap), 40.f + hpBar.y + 10.f, statusSize, statusSize),
                    tex((std::string("HUD/Status/") + kStatus[i] + ".png").c_str()), white, false);
    }

    // ---- area title (top-right) ----
    {
        const Value* at_ = member(layout, "areaTitle");
        const Vector2 offset = vec2(at_, "offset", Vector2(-48.f, 40.f));
        const float actSize = num(member(at_, "act"), "size", 15.f);
        const float nameSize = num(member(at_, "name"), "size", 34.f);
        const float objSize = num(member(at_, "objective"), "size", 18.f);
        const float width = 640.f;

        GameObject* area = b.node(main, "HUD_Area", pinned({ 1.f, 0.f }, offset, { width, actSize + nameSize + objSize + 30.f }));
        auto row = [&](float y, float h){ return Layout{ { 1.f, 0.f }, { 1.f, 0.f }, { 1.f, 0.f }, { 0.f, y }, { width, h } }; };
        b.label(area, "HUD_AreaAct", row(0.f, actSize + 4.f), "", "AshfallLabel", actSize, brass, H::Right);
        b.label(area, "HUD_AreaName", row(actSize + 6.f, nameSize + 6.f), "", "AshfallDisplay", nameSize, parchment, H::Right);
        GameObject* objective = b.node(area, "HUD_Objective", row(actSize + nameSize + 18.f, objSize + 6.f), false);
        b.image(objective, "HUD_ObjectiveBullet", pinned({ 1.f, .5f }, { 0.f, 0.f }, { 16.f, 16.f }), tex("HUD/Bars/ornament_diamond_brass.png"));
        b.label(objective, "HUD_ObjectiveText", { { 0.f, 0.f }, { 1.f, 1.f }, { 0.f, .5f }, { 0.f, 0.f }, { -24.f, 0.f } },
                "", "AshfallUI", objSize, parchment, H::Right);
    }

    // ---- ability bar (bottom-centre) ----
    {
        const Value* ab = member(layout, "abilityBar");
        const Vector2 offset = vec2(ab, "offset", Vector2(0.f, -34.f));
        const Vector4 pad = edges(ab, "padding", Vector4(22.f, 16.f, 22.f, 22.f));   // top, right, bottom, left
        const float slot = num(ab, "slot", 68.f);
        const float sigSlot = num(ab, "signatureSlot", 84.f);
        const Vector2 plate(821.f, 122.f);   // HUD/Sized/ability_plate_821x122.png

        GameObject* barGO = b.image(main, "HUD_AbilityBar", pinned({ .5f, 1.f }, offset, plate), tex("HUD/Sized/ability_plate_821x122.png"));

        // Slots are spaced by `gap`; the leftover inner width is shared by the three group dividers.
        const float gap = num(ab, "gap", 10.f);
        float slotsWidth = 0.f;
        int slots = 0, dividers = 0;
        for (const SlotDef& s : kSlots){
            if (!s.id){ ++dividers; continue; }
            slotsWidth += s.signature ? sigSlot : slot;
            ++slots;
        }
        const float innerW = plate.x - pad.w - pad.y;
        const float innerH = plate.y - pad.x - pad.z;
        const int plainGaps = (slots - 1) - dividers;
        const float dividerSpan = std::max(2.f * gap + 1.f, (innerW - slotsWidth - plainGaps * gap) / std::max(1, dividers));
        const float used = slotsWidth + plainGaps * gap + dividers * dividerSpan;
        float x = pad.w + std::max(0.f, (innerW - used) * 0.5f);

        bool first = true;
        int dividerIndex = 0;
        for (const SlotDef& s : kSlots){
            if (!s.id){
                b.image(barGO, "HUD_Divider" + std::to_string(dividerIndex++),
                        at(x + (dividerSpan - 1.f) * 0.5f, pad.x + (innerH - 56.f) * 0.5f, 1.f, 56.f), tex("UI/Panels/divider_v.png"));
                x += dividerSpan;
                first = true;
                continue;
            }
            if (!first) x += gap;
            first = false;

            const float size = s.signature ? sigSlot : slot;
            const std::string px = std::to_string((int)size);
            const std::string n = std::string("HUD_Slot_") + s.id;
            // Vertically centred in the plate's inner area, which the signature slot fills.
            GameObject* sg = b.node(barGO, n, at(x, pad.x + (innerH - size) * 0.5f, size, size));

            b.image(sg, n + "_Bg", inset(0.f), tex(("HUD/Slots/slot_bg_" + px + ".png").c_str()));
            const float icon = std::round(size * 0.48f);
            b.image(sg, n + "_Icon", pinned({ .5f, .5f }, Vector2::Zero, { icon, icon }), tex((std::string("Icons/") + s.icon + ".png").c_str()), parchment);
            if (s.signature)
                b.bar(sg, n + "_SigCharge", inset(0.f), tex("HUD/Slots/slot_overlay_sigcharge.png"), Dir::BottomToTop, 0.f);
            b.bar(sg, n + "_Cooldown", inset(0.f), tex("HUD/Slots/slot_overlay_cooldown.png"), Dir::TopToBottom, 0.f);
            b.label(sg, n + "_CDText", inset(0.f), "", "AshfallUI", 24.f, parchment, H::Center);
            // Border textures carry 16 px of glow on every side.
            b.image(sg, n + "_Border", at(-16.f, -16.f, size + 32.f, size + 32.f), tex(("HUD/Slots/slot_border_ready_" + px + ".png").c_str()));
            b.image(sg, n + "_Pip0", at(5.f, 5.f, 12.f, 12.f), tex("HUD/Slots/tier_pip_on.png"), white, false);
            b.image(sg, n + "_Pip1", at(19.f, 5.f, 12.f, 12.f), tex("HUD/Slots/tier_pip_off.png"), white, false);

            // Key chips just below the slot. Both widths exist: AshfallHUD fills in the label from the game's key
            // table and shows the narrow chip (24 px) for one character, the wide one (44 px) for longer names.
            // keychip.png is a 24x20 nine-slice with 3 px caps, so the wide chip is drawn as cap / middle / cap.
            GameObject* chip = b.node(sg, n + "_Key", { { .5f, 1.f }, { .5f, 1.f }, { .5f, 0.f }, { 0.f, 1.f }, { 24.f, 20.f } });
            b.image(chip, n + "_KeyBg", inset(0.f), tex("HUD/Slots/keychip.png"));
            b.label(chip, n + "_KeyText", inset(0.f), "", "AshfallUI", 12.f, brassLight, H::Center);

            GameObject* wide = b.node(sg, n + "_KeyWide", { { .5f, 1.f }, { .5f, 1.f }, { .5f, 0.f }, { 0.f, 1.f }, { 44.f, 20.f } }, false);
            auto piece = [&](const char* suffix, const Layout& l, Vector4 src){
                GameObject* p = b.image(wide, n + suffix, l, tex("HUD/Slots/keychip.png"));
                auto* img = p->getComponent<ComponentImage>();
                img->useSourceRect = true;
                img->sourceRect = src;
            };
            piece("_KeyWideL", at(0.f, 0.f, 3.f, 20.f), Vector4(0.f, 0.f, 3.f, 20.f));
            piece("_KeyWideM", { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, Vector2::Zero, { -6.f, 0.f } }, Vector4(3.f, 0.f, 18.f, 20.f));
            piece("_KeyWideR", pinned({ 1.f, 0.f }, Vector2::Zero, { 3.f, 20.f }), Vector4(21.f, 0.f, 3.f, 20.f));
            b.label(wide, n + "_KeyWideText", inset(0.f), "", "AshfallUI", 12.f, brassLight, H::Center);

            x += size;
        }
    }

    // ---- salvage (bottom-right) ----
    {
        const Value* sv = member(layout, "salvage");
        const Vector2 offset = vec2(sv, "offset", Vector2(-48.f, -48.f));
        const float valueSize = num(member(sv, "value"), "size", 22.f);
        GameObject* salvage = b.image(main, "HUD_Salvage", pinned({ 1.f, 1.f }, offset, { 190.f, 44.f }), tex("HUD/Sized/salvage_panel_190x44.png"));
        b.image(salvage, "HUD_SalvageIcon", pinned({ 0.f, .5f }, { 14.f, 0.f }, { 22.f, 22.f }), tex("Icons/currency_salvage.png"));
        b.label(salvage, "HUD_SalvageText", { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, { 15.f, 0.f }, { -58.f, 0.f } },
                "0", "AshfallUI", valueSize, parchment, H::Right);
    }

    if (!PrefabManager::createPrefab(root, kAshfallHUDPrefab)){
        outMessage = std::string("could not write the ") + kAshfallHUDPrefab + " prefab: the built HUD_Root is left in the scene";
        return false;
    }

    destroySubtree(scene, root);
    outMessage = app->getFileSystem()->GetLibraryPath() + "Prefabs/" + kAshfallHUDPrefab + ".prefab";
    return true;
}
