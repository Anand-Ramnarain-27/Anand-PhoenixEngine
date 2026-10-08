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
    std::string tex(const std::string& relative){ return std::string(kPack) + relative; }

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
        const char* icon;      // path under Assets/Ashfall_UI/
        bool signature;
        bool party = false;    // the Q / R group: nodes are HUD_SlotQ / HUD_SlotR, not HUD_Slot_<id>
    };
    // Group separators ("|" in hud_layout.json's abilityBar.order) are the null ids.
    const SlotDef kSlots[] = {
        { "Light", "Icons/icon_sword.png", false }, { "Heavy", "Icons/icon_heavy.png", false }, { nullptr, nullptr, false },
        { "Dodge", "Icons/icon_dodge.png", false }, { "Block", "Icons/icon_shield.png", false }, { nullptr, nullptr, false },
        { "A1", "Icons/icon_bash.png", false }, { "A2", "Icons/icon_smite.png", false }, { "A3", "Icons/icon_aegis.png", false },
        { "A4", "Icons/icon_rally.png", false },
        { nullptr, nullptr, false },
        // Party (HUD_PARTY.md): Q swap shows who comes in; R is the Duo Finisher (locked / charging / ready).
        { "Q", "HUD/Party/icon_swap.png", false, true }, { "R", "HUD/Party/icon_duo_locked.png", false, true },
        { nullptr, nullptr, false },
        { "Signature", "Icons/icon_vow.png", true },
    };
    // Every group divider takes this much width; the plate is as wide as its slots need (ability_plate_<W>x122.png,
    // made by Assets/Ashfall_UI/_tools/make_party_hud.py for the same sum).
    constexpr float kDividerSpan = 35.f;

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

    using H = ComponentLabel::HAlign;
    using V = ComponentLabel::VAlign;
    using Dir = ComponentProgressBar::FillDirection;

    /// One HUD build: the layout and colour tokens it reads, and a method per HUD region.
    struct HudBuild {
        Builder b;
        const Value* layout;
        std::string assets;   // absolute path of Assets/Ashfall_UI/
        Vector4 parchment, ash, brass, brassLight;
        Vector4 white = Vector4(1.f, 1.f, 1.f, 1.f);

        void playerFrame(GameObject* main);
        void benchedCharacter(GameObject* player, const Value* party, const Value* bench, Vector2 benchAt,
                              float benchPortrait);
        void areaTitle(GameObject* main);
        void abilityBar(GameObject* main);
        void salvage(GameObject* main);
        void partyPanels(GameObject* main);
        void bossBar(GameObject* main);
        void enemyBars(GameObject* main);
    };
}

/// Top-left: the active character's medallion, name, HP / Mana bars, status icons and the bench row under it.
void HudBuild::playerFrame(GameObject* main){
    const Value* pf = member(layout, "playerFrame");
    const Vector2 offset = vec2(pf, "offset", Vector2(40.f, 36.f));
    const float medallion = num(pf, "medallion", 120.f);
    const float gap = num(pf, "gap", 16.f);
    const Vector2 hpBar = vec2(pf, "hpBar", Vector2(440.f, 24.f));
    const float nameSize = num(member(pf, "name"), "size", 28.f);
    const float statusSize = num(member(pf, "statusRow"), "size", 30.f);
    const float statusGap = num(member(pf, "statusRow"), "gap", 8.f);
    const float textX = medallion + gap;

    // Tall enough for the benched portrait row under the medallion (hud_layout.json "party.bench").
    const Value* party = member(layout, "party");
    const Value* bench = member(party, "bench");
    const Vector2 benchAt = vec2(bench, "offset", Vector2(52.f, 134.f));
    const float benchPortrait = num(bench, "portrait", 64.f);
    GameObject* player = b.node(main, "HUD_Player", pinned({ 0.f, 0.f }, offset, { textX + hpBar.x, benchAt.y + benchPortrait + 8.f }));

    GameObject* med = b.node(player, "HUD_Medallion", at(0.f, 0.f, medallion, medallion));
    b.image(med, "HUD_VowTrack", inset(0.f), tex("HUD/Portrait/medallion_track_120.png"));
    b.image(med, "HUD_VowRing", inset(0.f), tex("HUD/Portrait/VowFill/vow_ring_fill_000_120.png"), Vector4(1.f, 1.f, 1.f, .55f));
    // vow_ring_glow_120 is 160x160 with 20 px of glow on every side.
    b.image(med, "HUD_VowGlow", at(-20.f, -20.f, medallion + 40.f, medallion + 40.f), tex("HUD/Portrait/vow_ring_glow_120.png"), white, false);
    const float portrait = 94.f, portraitAt = (medallion - portrait) * 0.5f;
    b.image(med, "HUD_PortraitBg", at(portraitAt, portraitAt, portrait, portrait), tex("HUD/Portrait/portrait_bg_94.png"));
    b.image(med, "HUD_Portrait", at(portraitAt, portraitAt, portrait, portrait), tex("HUD/Portrait/monogram_S_94.png"));   // TODO(hud): monogram placeholder until character portraits exist
    b.image(med, "HUD_PortraitRim", at(portraitAt, portraitAt, portrait, portrait), tex("HUD/Portrait/portrait_rim_brass_94.png"));
    // Link swap flash: a brass edge on the incoming portrait (link_edge_94 carries 16 px of glow on every side).
    b.image(med, "HUD_LinkEdge", at(portraitAt - 16.f, portraitAt - 16.f, portrait + 32.f, portrait + 32.f),
            tex("HUD/Party/link_edge_94.png"), white, false);

    b.label(player, "HUD_Name", at(textX, 0.f, hpBar.x, nameSize + 6.f), "Sera Vantry", "AshfallDisplay", nameSize, parchment);
    b.label(player, "HUD_Class", at(textX, 0.f, hpBar.x, nameSize + 6.f), "PALADIN", "AshfallLabel", 15.f, ash, H::Right);
    // "LINK · <name>" for a moment after a Link swap, in the class label's place (right end of the name row).
    GameObject* linkTag = b.image(player, "HUD_LinkTag", at(textX + hpBar.x - 220.f, 4.f, 220.f, 24.f), tex("HUD/Party/link_tag_220x24.png"), white, false);
    b.label(linkTag, "HUD_LinkTagKicker", at(10.f, 0.f, 44.f, 24.f), "LINK", "AshfallLabel", 12.f, brassLight);
    b.label(linkTag, "HUD_LinkTagName", at(54.f, 0.f, 158.f, 24.f), "", "AshfallUI", 16.f, parchment);

    // The frame's 2 px border stays visible: the bars sit inside it.
    GameObject* hp = b.image(player, "HUD_HPFrame", at(textX, 40.f, hpBar.x, hpBar.y), tex("HUD/Sized/hp_frame_440x24.png"));
    b.bar(hp, "HUD_HPTrail", inset(2.f), tex("HUD/Bars/bar_fill_ember_trail.png"), Dir::LeftToRight, 1.f);
    b.bar(hp, "HUD_HP", inset(2.f), tex("HUD/Bars/bar_fill_blood.png"), Dir::LeftToRight, 1.f);
    b.bar(hp, "HUD_HPLow", inset(2.f), tex("HUD/Bars/bar_fill_blood_low.png"), Dir::LeftToRight, 1.f, false);
    b.label(hp, "HUD_HPText", { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, { 0.f, 0.f }, { -16.f, 0.f } },
            "100 / 100", "AshfallUI", 15.f, parchment, H::Right);

    // Resource bar (Oskar's Mana) under the HP bar; AshfallHUD shows it only for a character with a resource.
    const Vector2 resourceBar = vec2(pf, "resourceBar", Vector2(343.f, 10.f));
    const float resourceY = 40.f + hpBar.y + 6.f;
    GameObject* res = b.image(player, "HUD_ManaFrame", at(textX, resourceY, resourceBar.x, resourceBar.y),
                              tex("HUD/Sized/resource_frame_343x10.png"), white, false);
    b.bar(res, "HUD_Mana", inset(1.f), tex("HUD/Bars/bar_fill_mana.png"), Dir::LeftToRight, 1.f);

    static const char* kStatus[] = { "status_aegis", "status_marked", "status_snare", "status_stagger" };
    for (int i = 0; i < 4; ++i)
        b.image(player, "HUD_Status" + std::to_string(i), at(textX + i * (statusSize + statusGap), resourceY + resourceBar.y + 8.f, statusSize, statusSize),
                tex((std::string("HUD/Status/") + kStatus[i] + ".png").c_str()), white, false);

    benchedCharacter(player, party, bench, benchAt, benchPortrait);
}

/// The benched character (HUD_PARTY.md): 64 px portrait with a Q badge, name + state, its own HP (with the slow
/// ember-trail recovery as a ghost), Mana when it has one, and the five Kinship pips beside it.
void HudBuild::benchedCharacter(GameObject* player, const Value* party, const Value* bench, Vector2 benchAt,
                                float benchPortrait){
    // TODO(hud): monogram portraits stand in until the characters' portrait art exists.
    const float gapB = num(bench, "gap", 12.f);
    const float nameB = num(member(bench, "name"), "size", 18.f);
    const Vector2 hpB = vec2(bench, "hpBar", Vector2(200.f, 10.f));
    const Vector2 manaB = vec2(bench, "manaBar", Vector2(200.f, 5.f));
    const Value* kin = member(party, "kinship");
    const float pip = num(kin, "pip", 14.f), pipGap = num(kin, "gap", 10.f), kinMargin = num(kin, "marginLeft", 10.f);
    const float colX = benchPortrait + gapB;
    const float pitch = pip + pipGap;
    const float kinW = 4.f * pitch + 30.f;

    GameObject* bw = b.node(player, "HUD_Bench", at(benchAt.x, benchAt.y, colX + hpB.x + kinMargin + kinW, benchPortrait), false);
    GameObject* bp = b.node(bw, "HUD_BenchPortrait", at(0.f, 0.f, benchPortrait, benchPortrait));
    b.image(bp, "HUD_BenchTrack", inset(0.f), tex("HUD/Party/bench_track_64.png"));
    b.image(bp, "HUD_BenchVowRing", inset(0.f), tex("HUD/Party/VowFill64/vow_ring_fill_000_64.png"), Vector4(1.f, 1.f, 1.f, .55f), false);
    const float inner = 50.f, innerAt = (benchPortrait - inner) * 0.5f;
    b.image(bp, "HUD_BenchPortraitBg", at(innerAt, innerAt, inner, inner), tex("HUD/Portrait/portrait_bg_50.png"));
    b.image(bp, "HUD_BenchMonogram", at(innerAt, innerAt, inner, inner), tex("HUD/Portrait/monogram_O_50.png"));
    b.bar(bp, "HUD_BenchSwapCD", at(innerAt, innerAt, inner, inner), tex("HUD/Slots/slot_overlay_cooldown.png"), Dir::TopToBottom, 0.f);
    b.image(bp, "HUD_BenchRim", at(innerAt, innerAt, inner, inner), tex("HUD/Portrait/portrait_rim_brass_50.png"));
    b.image(bp, "HUD_BenchDown", at(innerAt, innerAt, inner, inner), tex("HUD/Portrait/portrait_rim_low_50.png"), white, false);
    GameObject* q = b.image(bp, "HUD_BenchQ", at(benchPortrait - 18.f, benchPortrait - 12.f, 24.f, 18.f), tex("HUD/Party/badge_key_24x18.png"));
    b.label(q, "HUD_BenchQText", inset(0.f), "Q", "AshfallUI", 12.f, brassLight, H::Center);

    const float colH = nameB + 4.f + 4.f + hpB.y + 4.f + manaB.y;
    const float y0 = std::max(0.f, (benchPortrait - colH) * 0.5f);
    b.label(bw, "HUD_BenchName", at(colX, y0 - 2.f, hpB.x, nameB + 4.f), "", "AshfallDisplay", nameB, parchment);
    b.label(bw, "HUD_BenchState", at(colX, y0 - 2.f, hpB.x, nameB + 4.f), "", "AshfallLabel", 12.f, ash, H::Right);
    const float hpY = y0 + nameB + 8.f;
    GameObject* bhp = b.image(bw, "HUD_BenchHP", at(colX, hpY, hpB.x, hpB.y), tex("HUD/Party/bench_hp_frame_200x10.png"));
    b.bar(bhp, "HUD_BenchHPHeal", inset(1.f), tex("HUD/Bars/bar_fill_heal_ghost.png"), Dir::LeftToRight, 1.f);
    b.bar(bhp, "HUD_BenchHPFill", inset(1.f), tex("HUD/Bars/bar_fill_blood.png"), Dir::LeftToRight, 1.f);
    GameObject* bmana = b.image(bw, "HUD_BenchMana", at(colX, hpY + hpB.y + 4.f, manaB.x, manaB.y), tex("HUD/Party/bench_mana_frame_200x5.png"), white, false);
    b.bar(bmana, "HUD_BenchManaFill", inset(1.f), tex("HUD/Bars/bar_fill_mana.png"), Dir::LeftToRight, 1.f);

    // Kinship: 14 px diamonds on 30 px textures (8 px of glow padding), with the label under them.
    GameObject* kinGO = b.node(bw, "HUD_Kinship", at(colX + hpB.x + kinMargin, 0.f, kinW, benchPortrait), false);
    for (int i = 0; i < 5; ++i)
        b.image(kinGO, "HUD_Kinship_" + std::to_string(i), at(i * pitch, 12.f, 30.f, 30.f), tex("HUD/Party/kinship_pip_empty.png"));
    b.label(kinGO, "HUD_KinshipLabel", at(0.f, 42.f, kinW, 16.f), "KINSHIP", "AshfallLabel", 11.f, brassLight, H::Center);
}

/// Top-right: act, area name and the current objective.
void HudBuild::areaTitle(GameObject* main){
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

/// Bottom-centre: the ability plate with its slots, group dividers and key chips.
void HudBuild::abilityBar(GameObject* main){
    const Value* ab = member(layout, "abilityBar");
    const Vector2 offset = vec2(ab, "offset", Vector2(0.f, -34.f));
    const Vector4 pad = edges(ab, "padding", Vector4(22.f, 16.f, 22.f, 22.f));   // top, right, bottom, left
    const float slot = num(ab, "slot", 68.f);
    const float sigSlot = num(ab, "signatureSlot", 84.f);

    // Slots are spaced by `gap` inside a group and kDividerSpan between groups; the plate is sized to fit them.
    const float gap = num(ab, "gap", 10.f);
    float slotsWidth = 0.f;
    int slots = 0, dividers = 0;
    for (const SlotDef& s : kSlots){
        if (!s.id){ ++dividers; continue; }
        slotsWidth += s.signature ? sigSlot : slot;
        ++slots;
    }
    const int plainGaps = (slots - 1) - dividers;
    const Vector2 plate(std::round(slotsWidth + plainGaps * gap + dividers * kDividerSpan + pad.w + pad.y), 122.f);
    const std::string plateTex = "HUD/Sized/ability_plate_" + std::to_string((int)plate.x) + "x122.png";
    if (!std::ifstream(assets + plateTex).good())
        PHX_LOG(Editor, Warning, "[HUD] %s%s is missing: run Assets/Ashfall_UI/_tools/make_party_hud.py", assets.c_str(), plateTex.c_str());

    GameObject* barGO = b.image(main, "HUD_AbilityBar", pinned({ .5f, 1.f }, offset, plate), tex(plateTex));

    const float innerW = plate.x - pad.w - pad.y;
    const float innerH = plate.y - pad.x - pad.z;
    const float dividerSpan = kDividerSpan;
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
        const std::string n = std::string(s.party ? "HUD_Slot" : "HUD_Slot_") + s.id;
        // Vertically centred in the plate's inner area, which the signature slot fills.
        GameObject* sg = b.node(barGO, n, at(x, pad.x + (innerH - size) * 0.5f, size, size));

        b.image(sg, n + "_Bg", inset(0.f), tex(("HUD/Slots/slot_bg_" + px + ".png").c_str()));
        const float icon = std::round(size * 0.48f);
        b.image(sg, n + "_Icon", pinned({ .5f, .5f }, Vector2::Zero, { icon, icon }), tex(s.icon), parchment);
        if (s.signature)
            b.bar(sg, n + "_SigCharge", inset(0.f), tex("HUD/Slots/slot_overlay_sigcharge.png"), Dir::BottomToTop, 0.f);
        if (s.party && std::string(s.id) == "R")   // Kinship filling up while R charges
            b.bar(sg, n + "_Charge", inset(0.f), tex("HUD/Slots/slot_overlay_sigcharge.png"), Dir::BottomToTop, 0.f);
        b.bar(sg, n + "_Cooldown", inset(0.f), tex("HUD/Slots/slot_overlay_cooldown.png"), Dir::TopToBottom, 0.f);
        b.label(sg, n + "_CDText", inset(0.f), "", "AshfallUI", 24.f, parchment, H::Center);
        // Border textures carry 16 px of glow on every side.
        b.image(sg, n + "_Border", at(-16.f, -16.f, size + 32.f, size + 32.f), tex(("HUD/Slots/slot_border_ready_" + px + ".png").c_str()));
        b.image(sg, n + "_Pip0", at(5.f, 5.f, 12.f, 12.f), tex("HUD/Slots/tier_pip_on.png"), white, false);
        b.image(sg, n + "_Pip1", at(19.f, 5.f, 12.f, 12.f), tex("HUD/Slots/tier_pip_off.png"), white, false);
        if (s.party && std::string(s.id) == "Q")   // who comes in: the partner's sigil, top-right
            b.image(sg, n + "_Badge", at(size - 18.f, 4.f, 14.f, 14.f), tex("HUD/Party/sigil_oskar_soft_14.png"), white, false);

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

/// Bottom-right: the salvage counter.
void HudBuild::salvage(GameObject* main){
    const Value* sv = member(layout, "salvage");
    const Vector2 offset = vec2(sv, "offset", Vector2(-48.f, -48.f));
    const float valueSize = num(member(sv, "value"), "size", 22.f);
    GameObject* salvage = b.image(main, "HUD_Salvage", pinned({ 1.f, 1.f }, offset, { 190.f, 44.f }), tex("HUD/Sized/salvage_panel_190x44.png"));
    b.image(salvage, "HUD_SalvageIcon", pinned({ 0.f, .5f }, { 14.f, 0.f }, { 22.f, 22.f }), tex("Icons/currency_salvage.png"));
    b.label(salvage, "HUD_SalvageText", { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, { 15.f, 0.f }, { -58.f, 0.f } },
            "0", "AshfallUI", valueSize, parchment, H::Right);
}

/// Party panels (HUD_PARTY.md), all hidden until AshfallHUD shows them.
void HudBuild::partyPanels(GameObject* main){
    const Value* party = member(layout, "party");
    auto box = [&](const char* key, Vector2 anchor, Vector2 offset, Vector2 size){
        const Value* v = member(party, key);
        return pinned(anchor, vec2(v, "offset", offset), vec2(v, "size", size));
    };

    // First-time prompt line, lower centre: sigil(s) + key badge + one line of text (fed by the tutorial prompts).
    const float lineText = num(member(member(party, "promptLine"), "text"), "size", 19.f);
    GameObject* line = b.image(main, "HUD_PromptLine", box("promptLine", { .5f, 1.f }, { 0.f, -226.f }, { 760.f, 46.f }),
                               tex("HUD/Party/prompt_line_760x46.png"), white, false);
    b.image(line, "HUD_PromptSigil0", at(20.f, 10.f, 26.f, 26.f), tex("HUD/Party/sigil_oskar_soft_26.png"));
    b.image(line, "HUD_PromptSigil1", at(50.f, 10.f, 26.f, 26.f), tex("HUD/Party/sigil_sera_soft_26.png"), white, false);
    GameObject* lineKey = b.image(line, "HUD_PromptKey", at(84.f, 11.f, 30.f, 24.f), tex("HUD/Prompts/keycap_brass.png"));
    b.label(lineKey, "HUD_PromptKeyText", inset(0.f), "Q", "AshfallUI", 16.f, brassLight, H::Center);
    b.label(line, "HUD_PromptText", at(126.f, 0.f, 620.f, 46.f), "", "AshfallUI", lineText, parchment);

    // Interact prompt ("world" prompt drawn screen-space: scripts have no world-to-screen call yet).
    const float worldText = num(member(member(party, "worldPrompt"), "text"), "size", 17.f);
    GameObject* world = b.image(main, "HUD_WorldPrompt", box("worldPrompt", { .5f, 1.f }, { 0.f, -330.f }, { 360.f, 40.f }),
                                tex("HUD/Party/world_prompt_360x40.png"), white, false);
    GameObject* worldKey = b.image(world, "HUD_WorldPromptKey", at(10.f, 8.f, 24.f, 24.f), tex("HUD/Prompts/keycap_brass.png"));
    b.label(worldKey, "HUD_WorldPromptKeyText", inset(0.f), "E", "AshfallUI", 15.f, brassLight, H::Center);
    b.label(world, "HUD_WorldPromptText", at(44.f, 0.f, 270.f, 40.f), "", "AshfallUI", worldText, parchment);
    b.image(world, "HUD_WorldPromptSigil", at(324.f, 9.f, 22.f, 22.f), tex("HUD/Party/sigil_sera_soft_26.png"));
    b.label(world, "HUD_WorldPromptNeeds", at(0.f, 44.f, 360.f, 20.f), "", "AshfallUI", 14.f, ash, H::Center, V::Middle, false);

    // Both-puzzle progress plate under the area title: title, 3 step rows, "+N" for more.
    const int rows = (int)num(member(party, "puzzlePlate"), "rows", 3.f);
    GameObject* plateGO = b.image(main, "HUD_PuzzlePlate", box("puzzlePlate", { 1.f, 0.f }, { -48.f, 150.f }, { 300.f, 150.f }),
                                  tex("HUD/Party/puzzle_plate_300x150.png"), white, false);
    b.label(plateGO, "HUD_PuzzleTitle", at(16.f, 10.f, 268.f, 24.f), "", "AshfallDisplay", 18.f, parchment);
    for (int i = 0; i < rows; ++i){
        const std::string r = "HUD_PuzzleRow_" + std::to_string(i);
        GameObject* row = b.node(plateGO, r, at(16.f, 42.f + i * 28.f, 268.f, 26.f));
        b.image(row, r + "_Sigil", at(0.f, 2.f, 22.f, 22.f), tex("HUD/Party/sigil_sera_soft_26.png"));
        b.label(row, r + "_Label", at(32.f, 0.f, 200.f, 26.f), "", "AshfallUI", 16.f, parchment);
        b.image(row, r + "_Mark", at(248.f, 5.f, 16.f, 16.f), tex("HUD/Party/plate_diamond_open_16.png"));
    }
    b.label(plateGO, "HUD_PuzzleMore", at(16.f, 42.f + rows * 28.f, 268.f, 18.f), "", "AshfallUI", 13.f, ash, H::Left, V::Middle, false);

    // Oskar's Journal toast, top centre, below the boss bar's block so the two never overlap.
    GameObject* toast = b.image(main, "HUD_JournalToast", box("journalToast", { .5f, 0.f }, { 0.f, 220.f }, { 380.f, 64.f }),
                                tex("HUD/Party/journal_toast_380x64.png"), white, false);
    b.image(toast, "HUD_JournalIcon", at(16.f, 12.f, 40.f, 40.f), tex("HUD/Party/icon_book.png"), brassLight);
    b.label(toast, "HUD_JournalKicker", at(68.f, 8.f, 300.f, 20.f), "OSKAR'S JOURNAL", "AshfallLabel", 12.f, brass);
    b.label(toast, "HUD_JournalText", at(68.f, 28.f, 300.f, 28.f), "", "AshfallUI", 18.f, parchment);
}

/// Boss bar (hud_layout.json "bossBar"), top centre: name, subtitle, a long HP bar (rust frame, blood fill over
/// the ember recent-damage trail), diamond end caps, phase notches (placed by the script from the boss's phase
/// thresholds) and a poise strip under it. Hidden until a boss fight starts (GameScript UI/BossBar). The Journal
/// toast sits below this block (y 220).
void HudBuild::bossBar(GameObject* main){
    const Value* bb = member(layout, "bossBar");
    const Vector2 offset = vec2(bb, "offset", Vector2(0.f, 40.f));
    const float nameSize = num(member(bb, "name"), "size", 40.f);
    const float subSize = num(member(bb, "subtitle"), "size", 15.f);
    const Vector2 barSize = vec2(bb, "bar", Vector2(940.f, 20.f));
    const Vector2 poiseSize = vec2(bb, "poise", Vector2(940.f, 4.f));
    const float nameH = nameSize + 6.f, subH = subSize + 4.f;
    const float barY = nameH + subH + 4.f;
    const float poiseY = barY + barSize.y + 4.f;
    const float height = poiseY + poiseSize.y;
    GameObject* boss = b.node(main, "HUD_BossBar", pinned({ .5f, 0.f }, offset, { barSize.x + 48.f, height }), false);
    auto row = [&](float y, float h){ return Layout{ { 0.f, 0.f }, { 0.f, 0.f }, { 0.f, 0.f }, { 0.f, y }, { barSize.x + 48.f, h } }; };
    b.label(boss, "HUD_BossName", row(0.f, nameH), "", "AshfallDisplay", nameSize, parchment, H::Center);
    b.label(boss, "HUD_BossSubtitle", row(nameH, subH), "", "AshfallLabel", subSize, brass, H::Center);
    GameObject* frame = b.image(boss, "HUD_BossFrame", at(24.f, barY, barSize.x, barSize.y), tex("HUD/Bars/bar_frame_rust.png"));
    b.bar(frame, "HUD_BossTrail", inset(2.f), tex("HUD/Bars/bar_fill_ember_trail.png"), Dir::LeftToRight, 1.f);
    b.bar(frame, "HUD_BossHP", inset(2.f), tex("HUD/Bars/bar_fill_blood.png"), Dir::LeftToRight, 1.f);
    for (int i = 0; i < 3; ++i)   // brass phase notches, 2 x 32, centred on the bar; the script places / hides them
        b.image(frame, "HUD_BossNotch_" + std::to_string(i), pinned({ 0.f, .5f }, { 0.f, 0.f }, { 2.f, 32.f }),
                tex("HUD/Bars/bar_notch_brass.png"), white, false);
    b.image(boss, "HUD_BossCapL", at(4.f, barY + (barSize.y - 16.f) * 0.5f, 16.f, 16.f), tex("HUD/Bars/ornament_diamond_rust.png"));
    b.image(boss, "HUD_BossCapR", at(barSize.x + 28.f, barY + (barSize.y - 16.f) * 0.5f, 16.f, 16.f), tex("HUD/Bars/ornament_diamond_rust.png"));
    GameObject* poiseBg = b.image(boss, "HUD_BossPoiseBg", at(24.f, poiseY, poiseSize.x, poiseSize.y), tex("HUD/Bars/bar_frame_black.png"));
    b.bar(poiseBg, "HUD_BossPoise", inset(0.f), tex("HUD/Bars/bar_fill_poise.png"), Dir::LeftToRight, 1.f);
}

/// Enemy overhead bars (hud_layout.json "enemyBar"): a pool AshfallHUD's EnemyBars places above enemies every
/// frame (Phoenix::UI::WorldToCanvas). Each bar is anchored at the canvas' top-left with its pivot at the bottom
/// centre, so its position is the canvas point just above the enemy's head. Built at elite size; the script
/// resizes a regular bar and swaps the faction frame. Hidden until an enemy needs it.
void HudBuild::enemyBars(GameObject* main){
    const Value* eb = member(layout, "enemyBar");
    const int pool = (int)num(eb, "pool", 12.f);
    const Vector2 elite = vec2(eb, "elite", Vector2(132.f, 8.f));
    const Vector2 poise = vec2(eb, "elitePoise", Vector2(132.f, 3.f));
    const float sigil = num(eb, "sigil", 14.f);
    GameObject* layer = b.node(main, "HUD_EnemyBars", inset(0.f));
    for (int i = 0; i < pool; ++i){
        const std::string n = "HUD_EnemyBar_" + std::to_string(i);
        GameObject* bar = b.node(layer, n, { { 0.f, 0.f }, { 0.f, 0.f }, { .5f, 1.f }, Vector2::Zero, { elite.x, elite.y + 2.f + poise.y } }, false);
        GameObject* frame = b.image(bar, n + "_Frame", at(0.f, 0.f, elite.x, elite.y), tex("HUD/Sized/elite_frame_rust_132x8.png"));
        b.bar(frame, n + "_Trail", inset(1.f), tex("HUD/Bars/bar_fill_ember_trail.png"), Dir::LeftToRight, 1.f);
        b.bar(frame, n + "_HP", inset(1.f), tex("HUD/Bars/bar_fill_blood.png"), Dir::LeftToRight, 1.f);
        GameObject* poiseBg = b.image(bar, n + "_PoiseBg", at(0.f, elite.y + 2.f, poise.x, poise.y), tex("HUD/Sized/elite_poise_bg_132x3.png"), white, false);
        b.bar(poiseBg, n + "_Poise", inset(0.f), tex("HUD/Bars/bar_fill_poise.png"), Dir::LeftToRight, 1.f);
        // Affinity sigil left of the bar, buff marker (war drums) right of it.
        b.image(bar, n + "_Sigil", pinned({ 0.f, 0.f }, { -sigil - 4.f, (elite.y - sigil) * 0.5f }, { sigil, sigil }),
                tex("HUD/Party/sigil_oskar_soft_14.png"), white, false);
        b.image(bar, n + "_Buff", pinned({ 1.f, 0.f }, { sigil + 4.f, (elite.y - sigil) * 0.5f }, { sigil, sigil }),
                tex("HUD/Status/status_war_drums.png"), white, false);
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
        PHX_LOG(Editor, Error, "[HUD] %shud_layout.json missing or invalid: using built-in 1080p defaults", assets.c_str());
    if (!readJson(assets + "ui_tokens.json", tokensDoc))
        PHX_LOG(Editor, Error, "[HUD] %sui_tokens.json missing or invalid: using built-in colours", assets.c_str());

    const Value* layout = layoutDoc.IsObject() ? &layoutDoc : nullptr;
    const Value* colors = tokensDoc.IsObject() ? member(&tokensDoc, "colors") : nullptr;
    HudBuild hud{ { scene }, layout, assets,
                  color(colors, "parchment", "#EDE3CF"), color(colors, "ash", "#A2968A"),
                  color(colors, "brass", "#B89A62"), color(colors, "brassLight", "#DCC392") };
    Builder& b = hud.b;

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
    b.image(main, "HUD_LowHP", inset(0.f), tex("HUD/Screen/vignette_lowhp.png"), hud.white, /*visible=*/false);

    hud.playerFrame(main);
    hud.areaTitle(main);
    hud.abilityBar(main);
    hud.salvage(main);
    hud.partyPanels(main);
    hud.bossBar(main);
    hud.enemyBars(main);

    if (!PrefabManager::createPrefab(root, kAshfallHUDPrefab)){
        outMessage = std::string("could not write the ") + kAshfallHUDPrefab + " prefab: the built HUD_Root is left in the scene";
        return false;
    }

    destroySubtree(scene, root);
    outMessage = app->getFileSystem()->GetLibraryPath() + "Prefabs/" + kAshfallHUDPrefab + ".prefab";
    return true;
}
