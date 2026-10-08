#pragma once
// Imports glTF animations into Library/Animations/<model>/*.anim.

#include <string>

namespace tinygltf { class Model; }

namespace AnimationImporter {
    int ImportAll(const tinygltf::Model& gltfModel, const std::string& sceneName);
}
