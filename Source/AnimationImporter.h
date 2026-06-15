#pragma once
#include <string>

namespace tinygltf { class Model; }

namespace AnimationImporter {
    int ImportAll(const tinygltf::Model& gltfModel, const std::string& sceneName);
}
