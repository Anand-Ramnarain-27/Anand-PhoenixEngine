#pragma once
#include <string>

class SceneGraph;

void CreateTestScene(SceneGraph* scene,
                     const std::string& charModelPath,
                     const std::string& faceModelPath,
                     const std::string& smPath);

void ValidateAnimationSetup(SceneGraph* scene);
