#pragma once
#include <string>
#include <memory>

class ResourceAnimation;
namespace tinygltf { class Model; }

class AnimationImporter {
public:
    static std::unique_ptr<ResourceAnimation> Import(
        const tinygltf::Model& model,
        int animIndex,
        const std::string& clipName);

    static bool Save(const ResourceAnimation& anim, const std::string& path, class ModuleFileSystem* fsys);
    static bool Load(const std::string& path, ResourceAnimation& outAnim, class ModuleFileSystem* fsys);
};
