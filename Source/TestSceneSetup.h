#pragma once
#include <string>

class ModuleScene;

void CreateTestScene(ModuleScene* scene,
                     const std::string& charModelPath,
                     const std::string& faceModelPath,
                     const std::string& smPath);

void ValidateAnimationSetup(ModuleScene* scene);
