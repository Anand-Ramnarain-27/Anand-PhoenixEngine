#include "Globals.h"
#include "AnimationImporter.h"
#include "ResourceAnimation.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <algorithm>
#include <cassert>

// ?? helpers ????????????????????????????????????????????????????
const unsigned char* AnimationImporter::accessorPtr(
    const tinygltf::Model& model, int accIdx)
{
    const auto& acc = model.accessors[accIdx];
    const auto& view = model.bufferViews[acc.bufferView];
    return model.buffers[view.buffer].data.data()
        + view.byteOffset + acc.byteOffset;
}

int AnimationImporter::accessorStride(
    const tinygltf::Model& model, int accIdx, int defaultStride)
{
    const auto& acc = model.accessors[accIdx];
    int stride = (int)acc.ByteStride(model.bufferViews[acc.bufferView]);
    return stride ? stride : defaultStride;
}

// ?? read a float buffer ????????????????????????????????????????
// accessorPtr/accessorStride are public so this free function can call them.
static std::vector<float> readFloats(
    const tinygltf::Model& model, int accIdx, int components)
{
    const auto& acc = model.accessors[accIdx];
    const unsigned char* data =
        AnimationImporter::accessorPtr(model, accIdx);
    int stride =
        AnimationImporter::accessorStride(model, accIdx, components * 4);

    std::vector<float> out;
    out.reserve(acc.count * components);
    for (size_t i = 0; i < acc.count; ++i) {
        const float* f =
            reinterpret_cast<const float*>(data + i * stride);
        for (int c = 0; c < components; ++c)
            out.push_back(f[c]);
    }
    return out;
}

// ?? main import entry point ????????????????????????????????????
int AnimationImporter::ImportAll(
    const tinygltf::Model& model,
    const std::string& sceneName)
{
    if (model.animations.empty()) return 0;

    ModuleFileSystem* fs = app->getFileSystem();
    std::string outDir =
        fs->GetLibraryPath() + "Animations/" + sceneName + "/";
    fs->CreateDir(outDir.c_str());

    int written = 0;
    for (int animIdx = 0;
        animIdx < (int)model.animations.size(); ++animIdx)
    {
        const auto& gltfAnim = model.animations[animIdx];
        std::string outFile = outDir + std::to_string(animIdx) + ".anim";

        ResourceAnimation anim(0);
        std::string animName =
            gltfAnim.name.empty()
            ? (sceneName + "_anim_" + std::to_string(animIdx))
            : gltfAnim.name;
        anim.setName(animName);

        auto& channels = anim.getChannelsMutable();

        for (const auto& ch : gltfAnim.channels) {
            if (ch.target_node < 0 ||
                ch.target_node >= (int)model.nodes.size()) continue;

            const std::string& nodeName =
                model.nodes[ch.target_node].name;
            if (nodeName.empty()) continue;

            const auto& sampler = gltfAnim.samplers[ch.sampler];

            // Input accessor = timestamps in seconds
            std::vector<float> ts = readFloats(model, sampler.input, 1);

            AnimChannel& animCh = channels[nodeName];

            if (ch.target_path == "translation") {
                std::vector<float> vals =
                    readFloats(model, sampler.output, 3);
                animCh.posTimestamps = ts;
                animCh.positions.resize(vals.size() / 3);
                for (size_t i = 0; i < animCh.positions.size(); ++i)
                    animCh.positions[i] = {
                        vals[i * 3], vals[i * 3 + 1], vals[i * 3 + 2] };
            }
            else if (ch.target_path == "rotation") {
                std::vector<float> vals =
                    readFloats(model, sampler.output, 4);
                animCh.rotTimestamps = ts;
                animCh.rotations.resize(vals.size() / 4);
                for (size_t i = 0; i < animCh.rotations.size(); ++i)
                    // glTF quaternion order: (x, y, z, w) — same as SimpleMath
                    animCh.rotations[i] = {
                        vals[i * 4], vals[i * 4 + 1],
                        vals[i * 4 + 2], vals[i * 4 + 3] };
            }
            else if (ch.target_path == "scale") {
                std::vector<float> vals =
                    readFloats(model, sampler.output, 3);
                animCh.scaleTimestamps = ts;
                animCh.scales.resize(vals.size() / 3);
                for (size_t i = 0; i < animCh.scales.size(); ++i)
                    animCh.scales[i] = {
                        vals[i * 3], vals[i * 3 + 1], vals[i * 3 + 2] };
            }
        }

        anim.recomputeDuration();

        if (Save(anim, outFile))
            ++written;
        else
            LOG("AnimationImporter: failed to save '%s'", outFile.c_str());
    }
    LOG("AnimationImporter: wrote %d animations for '%s'",
        written, sceneName.c_str());
    return written;
}

// ?? binary save ????????????????????????????????????????????????
// FIX 1: use getChannels() (const overload) instead of getChannelsMutable()
//         so Save(const ResourceAnimation&) compiles without const_cast.
// FIX 2: Vector3/Quaternion members are not guaranteed to be packed floats
//         when addressed via &v.x with a stride trick. Copy to a plain
//         float[] first so serialization is safe and portable.
bool AnimationImporter::Save(
    const ResourceAnimation& anim,
    const std::string& file)
{
    // Helper: append raw bytes of any trivially-copyable array
    auto append = [](std::vector<char>& buf,
        const void* data, size_t byteCount)
        {
            const char* bytes = reinterpret_cast<const char*>(data);
            buf.insert(buf.end(), bytes, bytes + byteCount);
        };

    // Helper: write uint32 length-prefixed string + null terminator
    auto appendStr = [&](std::vector<char>& buf, const std::string& s)
        {
            uint32_t len = static_cast<uint32_t>(s.size());
            append(buf, &len, sizeof(uint32_t));
            buf.insert(buf.end(), s.begin(), s.end());
            buf.push_back('\0');
        };

    // FIX 1: const-correct channel access
    const auto& chans = anim.getChannels();

    std::vector<char> payload;

    for (const auto& [name, ch] : chans) {
        appendStr(payload, name);

        // ?? positions ?????????????????????????????????????????
        uint32_t np = static_cast<uint32_t>(ch.positions.size());
        append(payload, &np, sizeof(uint32_t));
        if (np) {
            append(payload, ch.posTimestamps.data(), np * sizeof(float));

            // FIX 2: flatten Vector3 array to a contiguous float buffer
            std::vector<float> posFlat;
            posFlat.reserve(np * 3);
            for (const auto& v : ch.positions) {
                posFlat.push_back(v.x);
                posFlat.push_back(v.y);
                posFlat.push_back(v.z);
            }
            append(payload, posFlat.data(), posFlat.size() * sizeof(float));
        }

        // ?? rotations ?????????????????????????????????????????
        uint32_t nr = static_cast<uint32_t>(ch.rotations.size());
        append(payload, &nr, sizeof(uint32_t));
        if (nr) {
            append(payload, ch.rotTimestamps.data(), nr * sizeof(float));

            // FIX 2: flatten Quaternion array
            std::vector<float> rotFlat;
            rotFlat.reserve(nr * 4);
            for (const auto& q : ch.rotations) {
                rotFlat.push_back(q.x);
                rotFlat.push_back(q.y);
                rotFlat.push_back(q.z);
                rotFlat.push_back(q.w);
            }
            append(payload, rotFlat.data(), rotFlat.size() * sizeof(float));
        }

        // ?? scales ????????????????????????????????????????????
        uint32_t ns = static_cast<uint32_t>(ch.scales.size());
        append(payload, &ns, sizeof(uint32_t));
        if (ns) {
            append(payload, ch.scaleTimestamps.data(), ns * sizeof(float));

            // FIX 2: flatten Vector3 scale array
            std::vector<float> scaleFlat;
            scaleFlat.reserve(ns * 3);
            for (const auto& v : ch.scales) {
                scaleFlat.push_back(v.x);
                scaleFlat.push_back(v.y);
                scaleFlat.push_back(v.z);
            }
            append(payload, scaleFlat.data(), scaleFlat.size() * sizeof(float));
        }
    }

    AnimHeader hdr;
    hdr.numChannels = static_cast<uint32_t>(chans.size());
    hdr.duration = anim.getDuration();

    return ImporterUtils::SaveBuffer(file, hdr, payload);
}

// ?? Load binary ????????????????????????????????????????????????
bool AnimationImporter::Load(
    const std::string& file,
    ResourceAnimation& outAnim)
{
    AnimHeader       hdr;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(file, hdr, raw)) {
        LOG("AnimationImporter::Load: failed to read '%s'", file.c_str());
        return false;
    }
    if (hdr.magic != 0x414E494D || hdr.version != 1) {
        LOG("AnimationImporter::Load: bad magic/version in '%s'",
            file.c_str());
        return false;
    }

    auto& channels = outAnim.getChannelsMutable();
    channels.reserve(hdr.numChannels);

    const char* cursor = raw.data();          // LoadBuffer gives payload only
    const char* end = raw.data() + raw.size();

    // Cursor helpers – each advances cursor
    auto readU32 = [&]() -> uint32_t {
        uint32_t v;
        std::memcpy(&v, cursor, sizeof(uint32_t));
        cursor += sizeof(uint32_t);
        return v;
        };
    auto readStr = [&]() -> std::string {
        uint32_t len = readU32();
        std::string s(cursor, len);
        cursor += len + 1;   // +1 for null terminator
        return s;
        };
    // Returns pointer into the raw buffer and advances cursor by n floats
    auto readFloatPtr = [&](uint32_t n) -> const float* {
        const float* p = reinterpret_cast<const float*>(cursor);
        cursor += n * sizeof(float);
        return p;
        };

    for (uint32_t i = 0; i < hdr.numChannels && cursor < end; ++i) {
        std::string  name = readStr();
        AnimChannel& ch = channels[name];

        // positions
        if (uint32_t np = readU32()) {
            const float* ts = readFloatPtr(np);
            ch.posTimestamps.assign(ts, ts + np);
            const float* px = readFloatPtr(np * 3);
            ch.positions.resize(np);
            for (uint32_t j = 0; j < np; ++j)
                ch.positions[j] = { px[j * 3], px[j * 3 + 1], px[j * 3 + 2] };
        }

        // rotations
        if (uint32_t nr = readU32()) {
            const float* ts = readFloatPtr(nr);
            ch.rotTimestamps.assign(ts, ts + nr);
            const float* rx = readFloatPtr(nr * 4);
            ch.rotations.resize(nr);
            for (uint32_t j = 0; j < nr; ++j)
                ch.rotations[j] = { rx[j * 4], rx[j * 4 + 1],
                                    rx[j * 4 + 2], rx[j * 4 + 3] };
        }

        // scales
        if (uint32_t ns = readU32()) {
            const float* ts = readFloatPtr(ns);
            ch.scaleTimestamps.assign(ts, ts + ns);
            const float* sx = readFloatPtr(ns * 3);
            ch.scales.resize(ns);
            for (uint32_t j = 0; j < ns; ++j)
                ch.scales[j] = { sx[j * 3], sx[j * 3 + 1], sx[j * 3 + 2] };
        }
    }

    outAnim.recomputeDuration();
    LOG("AnimationImporter: loaded '%s' (%.2fs, %zu channels)",
        file.c_str(), outAnim.getDuration(), channels.size());
    return true;
}