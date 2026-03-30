#include "Globals.h"
#include "SkinImporter.h"
#include "ResourceSkin.h"
#include "ImporterUtils.h"
#include "tiny_gltf.h"
#include <cstring>

static void writeStr2(std::vector<char>& b, const std::string& s) {
    uint32_t l = (uint32_t)s.size();
    b.insert(b.end(), (char*)&l, (char*)&l + 4);
    b.insert(b.end(), s.begin(), s.end());
}
static std::string readStr2(const char*& c) {
    uint32_t l; memcpy(&l, c, 4); c += 4;
    std::string s(c, l); c += l; return s;
}

bool SkinImporter::Import(const tinygltf::Model& model,
    int skinIndex,
    const std::string& outputFile) {
    if (skinIndex < 0 || skinIndex >= (int)model.skins.size()) return false;
    const auto& skin = model.skins[skinIndex];
    uint32_t jc = (uint32_t)skin.joints.size();

    // Read inverse bind matrices (float4x4 per joint).
    std::vector<Matrix> ibm(jc, Matrix::Identity);
    if (skin.inverseBindMatrices >= 0) {
        const auto& acc = model.accessors[skin.inverseBindMatrices];
        const auto& view = model.bufferViews[acc.bufferView];
        const float* src = reinterpret_cast<const float*>(
            model.buffers[view.buffer].data.data()
            + view.byteOffset + acc.byteOffset);
        for (uint32_t j = 0; j < jc && j < (uint32_t)acc.count; ++j)
            memcpy(&ibm[j], src + j * 16, 64);
    }

    SkinHeader hdr; hdr.jointCount = jc;
    std::vector<char> payload;
    // For each joint: length-prefixed name, then the 4x4 matrix (64 bytes).
    for (uint32_t j = 0; j < jc; ++j) {
        int nodeIdx = skin.joints[j];
        std::string nm = (nodeIdx < (int)model.nodes.size())
            ? model.nodes[nodeIdx].name : "";
        writeStr2(payload, nm);
        payload.insert(payload.end(), (char*)&ibm[j], (char*)&ibm[j] + 64);
    }
    return ImporterUtils::SaveBuffer(outputFile, hdr, payload);
}

bool SkinImporter::Load(const std::string& file, ResourceSkin& out) {
    SkinHeader hdr; std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(file, hdr, raw)) return false;
    if (hdr.magic != 0x4E494B53) return false;
    out.jointNames.resize(hdr.jointCount);
    out.inverseBindMatrices.resize(hdr.jointCount, Matrix::Identity);
    const char* cur = raw.data() + sizeof(SkinHeader);
    for (uint32_t j = 0; j < hdr.jointCount; ++j) {
        out.jointNames[j] = readStr2(cur);
        memcpy(&out.inverseBindMatrices[j], cur, 64); cur += 64;
    }
    return true;
}
