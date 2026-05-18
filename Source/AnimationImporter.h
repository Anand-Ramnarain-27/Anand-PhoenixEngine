#pragma once
#include <string>
#include <cstdint>

class ResourceAnimation;
namespace tinygltf { class Model; }

class AnimationImporter {
public:
    struct AnimHeader {
        uint32_t magic = 0x414E494D;  // 'ANIM'
        uint32_t version = 1;
        uint32_t numChannels = 0;
        float    duration = 0.0f;
    };

    /// Called from SceneImporter – imports all animations in the model.
    /// Returns number of animation files written.
    static int  ImportAll(const tinygltf::Model& model,
        const std::string& sceneName);

    /// Load a .anim file into a ResourceAnimation (fast binary path).
    static bool Load(const std::string& file,
        ResourceAnimation& outAnim);

    // FIX: public so the file-scope readFloats() helper can call them
    static const unsigned char* accessorPtr(
        const tinygltf::Model& model, int accIdx);

    static int accessorStride(
        const tinygltf::Model& model, int accIdx, int defaultStride);

private:
    static bool Save(const ResourceAnimation& anim,
        const std::string& file);
};