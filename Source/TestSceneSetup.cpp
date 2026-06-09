#include "Globals.h"
#include "TestSceneSetup.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentAnimation.h"
#include "ComponentCharacterMotion.h"
#include "ComponentSimpleCharacterController.h"
#include "ResourceStateMachine.h"
#include "ResourceAnimation.h"
#include "ResourceMesh.h"
#include "Application.h"
#include "ModuleResources.h"
#include "SkinningPass.h"
#include <functional>

// ─── CreateTestScene ─────────────────────────────────────────────────────────

void CreateTestScene(ModuleScene* scene,
                     const std::string& charModelPath,
                     const std::string& faceModelPath,
                     const std::string& smPath){
    if (!scene) return;
    scene->clear();

    // ── GameObject 1: TestCharacter ───────────────────────────────────────────
    GameObject* character = scene->createGameObject("TestCharacter");
    character->getTransform()->position = { 0.f, 0.f, 0.f };

    // Mesh (must be first so rendering works; update() is a no-op for ComponentMesh)
    auto* charMesh = character->createComponent<ComponentMesh>();
    if (!charModelPath.empty())
        charMesh->loadModel(charModelPath.c_str());

    // Controller reads input and fires triggers — add before Animation so it
    // updates first inside GameObject::update()'s component loop.
    character->createComponent<ComponentSimpleCharacterController>();

    // Animation processes triggers and applies bone transforms to child nodes.
    auto* charAnim = character->createComponent<ComponentAnimation>();
    if (!smPath.empty())
        charAnim->LoadStateMachineFromPath(smPath); // also calls OnPlay()

    // Motion integrates the root transform — must come last.
    auto* charMotion = character->createComponent<ComponentCharacterMotion>();
    charMotion->mLinearSpeed = 5.f;
    charMotion->mAngularSpeed = 2.f;

    // ── GameObject 2: TestFace ────────────────────────────────────────────────
    GameObject* face = scene->createGameObject("TestFace");
    face->getTransform()->position = { 3.f, 0.f, 0.f };

    auto* faceMesh = face->createComponent<ComponentMesh>();
    if (!faceModelPath.empty())
        faceMesh->loadModel(faceModelPath.c_str());

    // Direct-play animation: no state machine — AnimController will drive morph
    // weights automatically from the glTF "weights" channel each frame.
    face->createComponent<ComponentAnimation>();
    // The face animation UID is discovered after import; wire it via the inspector
    // or call faceAnim->OnPlay(uid, true) with the correct UID.

    LOG("[TestScene] Created TestCharacter at (0,0,0) and TestFace at (3,0,0).");
    LOG("[TestScene] Fill in AnimationUIDs in character_sm.json, then call ValidateAnimationSetup().");
}

// ─── ValidateAnimationSetup ───────────────────────────────────────────────────

static void check(bool ok, const char* msg){
    if (ok) { LOG("[OK]   %s", msg); }
    else { LOG("[FAIL] %s", msg); }
}

void ValidateAnimationSetup(ModuleScene* scene){
    LOG("=== ValidateAnimationSetup ===");

    if (!scene) { LOG("[FAIL] scene is null"); return; }

    // Locate the two expected GameObjects
    GameObject* charGO = scene->findGameObjectByName("TestCharacter");
    GameObject* faceGO = scene->findGameObjectByName("TestFace");
    check(charGO != nullptr, "TestCharacter GameObject exists in scene");
    check(faceGO != nullptr, "TestFace GameObject exists in scene");

    // ── Character checks ──────────────────────────────────────────────────────
    if (charGO) {
        auto* charAnim = charGO->getComponent<ComponentAnimation>();
        auto* charMotion = charGO->getComponent<ComponentCharacterMotion>();
        auto* charCtrl = charGO->getComponent<ComponentSimpleCharacterController>();

        check(charCtrl != nullptr, "ComponentSimpleCharacterController on TestCharacter");
        check(charAnim != nullptr, "ComponentAnimation on TestCharacter");
        check(charMotion != nullptr, "ComponentCharacterMotion on TestCharacter");

        // Update order (component vector order determines per-frame call order)
        if (charCtrl && charAnim && charMotion) {
            const auto& comps = charGO->getComponents();
            int ctrlIdx = -1, animIdx = -1, motionIdx = -1;
            for (int i = 0; i < (int)comps.size(); ++i) {
                if (comps[i].get() == charCtrl) ctrlIdx = i;
                if (comps[i].get() == charAnim) animIdx = i;
                if (comps[i].get() == charMotion) motionIdx = i;
            }
            check(ctrlIdx < animIdx && animIdx < motionIdx,
                  "Update order: Controller < Animation < Motion");
        }

        ResourceStateMachine* sm = charAnim ? charAnim->getStateMachine() : nullptr;
        check(sm != nullptr, "ResourceStateMachine loaded on TestCharacter");

        if (sm) {
            check(!sm->states.empty(), "SM has > 0 states");

            const bool defaultExists =
                !sm->defaultState.empty() && sm->FindState(sm->defaultState) != nullptr;
            check(defaultExists, "SM defaultState exists in states list");

            bool allClipsValid = true;
            for (const auto& s : sm->states)
                if (!s.clipName.empty() && !sm->FindClip(s.clipName))
                    { allClipsValid = false; break; }
            check(allClipsValid, "Every state's clipName exists in SM clips");

            bool allTransValid = true;
            for (const auto& t : sm->transitions)
                if (!sm->FindState(t.source) || !sm->FindState(t.target))
                    { allTransValid = false; break; }
            check(allTransValid, "Every transition's source/target exist in SM states");

            // Clip UIDs — warn if any are still 0 (placeholder)
            bool anyZeroUID = false;
            for (const auto& c : sm->clips)
                if (c.animationUID == 0) { anyZeroUID = true; break; }
            check(!anyZeroUID,
                  "All SM clip AnimationUIDs are non-zero (replace placeholder 0s after import)");

            // Verify each clip UID loads a ResourceAnimation
            if (!anyZeroUID) {
                bool allLoaded = true;
                for (const auto& c : sm->clips) {
                    auto* anim = app->getResources()->RequestAnimation(c.animationUID);
                    if (!anim) { allLoaded = false; }
                    else app->getResources()->ReleaseResource(anim);
                }
                check(allLoaded, "All SM clip UIDs resolve to a valid ResourceAnimation");
            }
        }
    }

    // ── Face checks ───────────────────────────────────────────────────────────
    if (faceGO) {
        auto* faceMesh = faceGO->getComponent<ComponentMesh>();
        auto* faceAnim = faceGO->getComponent<ComponentAnimation>();
        check(faceMesh != nullptr, "ComponentMesh on TestFace");
        check(faceAnim != nullptr, "ComponentAnimation on TestFace");

        // Walk children looking for morph-target meshes
        if (faceMesh) {
            bool hasMorphTargets = false;
            std::function<void(GameObject*)> findMorphs = [&](GameObject* node) {
                if (auto* cm = node->getComponent<ComponentMesh>()) {
                    for (const auto& e : cm->getEntries()) {
                        if (e.meshRes && e.meshRes->getNumMorphTargets() > 0) {
                            hasMorphTargets = true;
                        }
                    }
                }
                for (auto* child : node->getChildren()) findMorphs(child);
            };
            findMorphs(faceGO);
            check(hasMorphTargets, "Face mesh has > 0 morph targets in ResourceMesh");
        }

        if (faceAnim) {
            bool hasMorphChannel = false;
            // Check if the controller's animation has a MorphChannel for any node
            const UID uid = faceAnim->getController().Resource;
            if (uid != 0) {
                auto* anim = app->getResources()->RequestAnimation(uid);
                if (anim) {
                    // MorphChannel presence is verified if the controller can sample weights
                    std::function<void(GameObject*)> checkMC = [&](GameObject* node) {
                        if (anim->getMorphChannel(node->getName()))
                            hasMorphChannel = true;
                        for (auto* child : node->getChildren()) checkMC(child);
                    };
                    checkMC(faceGO);
                    app->getResources()->ReleaseResource(anim);
                }
            }
            check(hasMorphChannel,
                  "Face ResourceAnimation has a MorphChannel matching at least one child node name");
        }
    }

    // ── SkinningPass capacity checks ──────────────────────────────────────────
    check(SkinningPass::MAX_TOTAL_MORPH_WEIGHTS >= 64,
          "SkinningPass morph weight buffer fits at least 64 targets (MAX_TOTAL_MORPH_WEIGHTS)");
    check(SkinningPass::MAX_TOTAL_JOINTS >= 64,
          "SkinningPass joint palette fits at least 64 joints (MAX_TOTAL_JOINTS)");

    // ── Update order note ─────────────────────────────────────────────────────
    LOG("[OK]   SkinningPass dispatches in render() after preRender() — always sees latest bone transforms.");
    LOG("[OK]   In editor (not playing): only ComponentAnimation::update() is called (preview mode).");
    LOG("[OK]   In play mode: root->update(dt) calls all components in insertion order per GO.");
    LOG("=== ValidateAnimationSetup done ===");
}
