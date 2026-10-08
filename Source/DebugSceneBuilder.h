#pragma once
// Animation test scene: a character + face model driven by a state machine, and a check of its setup.

#include <string>

class SceneGraph;

void CreateTestScene(SceneGraph* scene,
                     const std::string& charModelPath,
                     const std::string& faceModelPath,
                     const std::string& smPath);

void ValidateAnimationSetup(SceneGraph* scene);
