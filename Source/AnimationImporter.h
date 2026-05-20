#pragma once
#include <string>

namespace tinygltf { class Model; }

namespace AnimationImporter {
    // Imports all animations from gltfModel into Library/Animations/<sceneName>/.
    // Returns the number of .anim files written.
    int ImportAll(const tinygltf::Model& gltfModel, const std::string& sceneName);
}
