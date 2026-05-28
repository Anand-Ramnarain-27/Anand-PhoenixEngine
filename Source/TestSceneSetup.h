#pragma once
#include <string>

class ModuleScene;

// Programmatically builds the two-GO animation test scene.
//
// charModelPath  - absolute path to the character glTF (the one with skeletal anims)
// faceModelPath  - absolute path to the face/morph glTF
// smPath         - absolute path to character_sm.json (Assets/StateMachines/character_sm.json)
//
// Component order on TestCharacter guarantees correct per-frame ordering:
//   SimpleCharacterController → ComponentAnimation → ComponentCharacterMotion
//
// After calling CreateTestScene(), edit the AnimationUIDs in character_sm.json to
// match the UIDs assigned when your glTF animations were imported, then call
// ValidateAnimationSetup() to confirm everything is wired.
void CreateTestScene(ModuleScene* scene,
                     const std::string& charModelPath,
                     const std::string& faceModelPath,
                     const std::string& smPath);

// Walks the scene and logs [OK] / [FAIL] for each animation system invariant.
// Safe to call at any time after CreateTestScene(); does not modify the scene.
void ValidateAnimationSetup(ModuleScene* scene);
