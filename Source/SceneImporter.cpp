#include "Globals.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
#include "AnimationImporter.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>

namespace {
    struct NodeFileHeader {
        uint32_t magic = 0x4E4F4445;
        uint32_t version = 2;          // v1: no skinIndex; v2: skinIndex added
        uint32_t nodeCount = 0;
        uint32_t rootCount = 0;
        uint32_t meshRangeCount = 0;
    };
    struct NodeFileMeshRange {
        uint32_t fileStart;
        uint32_t fileCount;
    };
    // v1 on-disk layout (read-only — used for backward compat)
    struct NodeFileEntryV1 {
        int32_t  parentIndex;
        int32_t  meshRangeIdx;
        uint32_t nameLen;
        float    t[3];
        float    r[4];
        float    s[3];
    };
    // v2 on-disk layout (current write format)
    struct NodeFileEntry {
        int32_t  parentIndex;
        int32_t  meshRangeIdx;
        int32_t  skinIndex;    // -1 if not skinned
        uint32_t nameLen;
        float    t[3];
        float    r[4];
        float    s[3];
    };

    struct SkinFileHeader {
        uint32_t magic     = 0x534B4E53; // 'SKNS'
        uint32_t version   = 1;
        uint32_t skinCount = 0;
        uint32_t pad       = 0;
    };

    struct TempNode {
        std::string name;
        int parentIdx;
        int gltfMeshIdx;
        int skinIdx;       // gltf node.skin (-1 if none)
        float t[3], r[4], s[3];
    };

    void visitGLTFNode(int ni, int parentIdx, const tinygltf::Model& model,
                       std::vector<TempNode>& nodes) {
        if (ni < 0 || ni >= (int)model.nodes.size()) return;
        const auto& n = model.nodes[ni];
        TempNode tn{};
        tn.name       = n.name.empty() ? ("Node_" + std::to_string(ni)) : n.name;
        tn.parentIdx  = parentIdx;
        tn.gltfMeshIdx = n.mesh;
        tn.skinIdx     = n.skin;

        if (n.matrix.size() == 16) {
            float fm[16];
            for (int i = 0; i < 16; ++i) fm[i] = (float)n.matrix[i];
            Matrix mat;
            memcpy(&mat, fm, 64);
            mat = mat.Transpose(); // GLTF is column-major, SimpleMath is row-major
            Vector3 t, s; Quaternion r;
            if (mat.Decompose(s, r, t)) {
                tn.t[0]=t.x; tn.t[1]=t.y; tn.t[2]=t.z;
                tn.r[0]=r.x; tn.r[1]=r.y; tn.r[2]=r.z; tn.r[3]=r.w;
                tn.s[0]=s.x; tn.s[1]=s.y; tn.s[2]=s.z;
            } else {
                tn.r[3]=1.f; tn.s[0]=tn.s[1]=tn.s[2]=1.f;
            }
        } else {
            tn.t[0] = n.translation.size()>=3 ? (float)n.translation[0] : 0.f;
            tn.t[1] = n.translation.size()>=3 ? (float)n.translation[1] : 0.f;
            tn.t[2] = n.translation.size()>=3 ? (float)n.translation[2] : 0.f;
            tn.r[0] = n.rotation.size()>=4 ? (float)n.rotation[0] : 0.f;
            tn.r[1] = n.rotation.size()>=4 ? (float)n.rotation[1] : 0.f;
            tn.r[2] = n.rotation.size()>=4 ? (float)n.rotation[2] : 0.f;
            tn.r[3] = n.rotation.size()>=4 ? (float)n.rotation[3] : 1.f;
            tn.s[0] = n.scale.size()>=3 ? (float)n.scale[0] : 1.f;
            tn.s[1] = n.scale.size()>=3 ? (float)n.scale[1] : 1.f;
            tn.s[2] = n.scale.size()>=3 ? (float)n.scale[2] : 1.f;
        }
        int myIdx = (int)nodes.size();
        nodes.push_back(tn);
        for (int child : n.children) visitGLTFNode(child, myIdx, model, nodes);
    }
}

bool SceneImporter::ImportFromLoadedGLTF(const tinygltf::Model& gltfModel, const std::string& sceneName, const std::string& basePath) {
    if (!CreateSceneDirectory(sceneName)) LOG("SceneImporter: Warning: CreateSceneDirectory returned false for %s (may already exist)", sceneName.c_str());
    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshFolder = fs->GetLibraryPath() + "Meshes/" + sceneName;
    std::string matFolder = fs->GetLibraryPath() + "Materials/" + sceneName;
    int meshIndex = 0;

    for (const auto& mesh : gltfModel.meshes) {
        for (const auto& prim : mesh.primitives) {

            int currentMeshIndex = meshIndex;

            if (!MeshImporter::Import(
                prim,
                gltfModel,
                ImporterUtils::IndexedPath(meshFolder, currentMeshIndex, ".mesh")))
            {
                LOG("SceneImporter: Failed to import mesh %d", currentMeshIndex);
            }

            if (prim.material < -1 || prim.material >= (int)gltfModel.materials.size()) {
                LOG("SceneImporter: Invalid material index %d on mesh %d", prim.material, currentMeshIndex);
            }

            meshIndex++;
        }
    }
    for (int k = 0; k < (int)gltfModel.materials.size(); k++) {
        const auto& mat = gltfModel.materials[k];

        MaterialImporter::Import(mat, gltfModel, sceneName, ImporterUtils::IndexedPath(matFolder, k, ".mat"), k, basePath);
    }
    if (!SaveSceneMetadata(sceneName, gltfModel)) { LOG("SceneImporter: Failed to save scene metadata"); return false; }
    SaveNodeMetadata(sceneName, gltfModel);
    SaveSkinMetadata(sceneName, gltfModel);
    AnimationImporter::ImportAll(gltfModel, sceneName);
    return true;
}

bool SceneImporter::LoadScene(const std::string& sceneName, std::unique_ptr<Model>& outModel) {
    ModuleFileSystem* fs = app->getFileSystem();
    std::string folder = fs->GetLibraryPath() + "Meshes/" + sceneName;
    if (!fs->Exists(folder.c_str())) { LOG("SceneImporter: Scene folder does not exist: %s", folder.c_str()); return false; }
    SceneHeader header;
    if (!LoadSceneMetadata(sceneName, header)) { LOG("SceneImporter: Failed to load metadata for %s", sceneName.c_str()); return false; }
    return true;
}

bool SceneImporter::CreateSceneDirectory(const std::string& sceneName) {
    ModuleFileSystem* fs = app->getFileSystem();
    std::string lib = fs->GetLibraryPath();
    fs->CreateDir((lib + "Meshes").c_str());
    fs->CreateDir((lib + "Materials").c_str());
    return fs->CreateDir((lib + "Meshes/" + sceneName).c_str()) && fs->CreateDir((lib + "Materials/" + sceneName).c_str());
}

bool SceneImporter::SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel) {
    SceneHeader header;
    std::vector<int32_t> matIndices;
    for (const auto& mesh : gltfModel.meshes) {
        for (const auto& prim : mesh.primitives) {

            header.meshCount++;

            int matIdx = prim.material;

            if (matIdx < 0 || matIdx >= (int)gltfModel.materials.size())
                matIdx = -1;

            matIndices.push_back(matIdx);
        }
    }
    header.materialCount = (uint32_t)gltfModel.materials.size();
    std::vector<char> payload(matIndices.size() * sizeof(int32_t));
    memcpy(payload.data(), matIndices.data(), payload.size());
    return ImporterUtils::SaveBuffer(app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta", header, payload);
}

bool SceneImporter::LoadSceneMetadata(const std::string& sceneName, SceneHeader& header) {
    std::vector<char> rawBuffer;
    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";
    if (!ImporterUtils::LoadBuffer(path, header, rawBuffer)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x53434E45)) { LOG("SceneImporter: Invalid scene metadata"); return false; }
    return true;
}

bool SceneImporter::SaveNodeMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel) {
    // Build mesh-range table: gltfMeshIndex -> (firstFileIndex, primitiveCount)
    std::vector<NodeFileMeshRange> meshRanges;
    uint32_t fi = 0;
    for (const auto& m : gltfModel.meshes) {
        meshRanges.push_back({fi, (uint32_t)m.primitives.size()});
        fi += (uint32_t)m.primitives.size();
    }

    std::vector<TempNode> nodes;
    std::vector<int32_t>  roots;

    if (!gltfModel.scenes.empty()) {
        int si = (gltfModel.defaultScene >= 0 && gltfModel.defaultScene < (int)gltfModel.scenes.size())
                 ? gltfModel.defaultScene : 0;
        for (int ni : gltfModel.scenes[si].nodes) {
            roots.push_back((int32_t)nodes.size());
            visitGLTFNode(ni, -1, gltfModel, nodes);
        }
    }

    // Build fixed-size entry array and a concatenated name blob
    std::vector<NodeFileEntry> entries;
    std::string nameBlob;
    for (const auto& tn : nodes) {
        NodeFileEntry e{};
        e.parentIndex  = tn.parentIdx;
        e.meshRangeIdx = (tn.gltfMeshIdx >= 0 && tn.gltfMeshIdx < (int)meshRanges.size())
                         ? tn.gltfMeshIdx : -1;
        e.skinIndex    = tn.skinIdx;
        e.nameLen = (uint32_t)tn.name.size();
        memcpy(e.t, tn.t, 12); memcpy(e.r, tn.r, 16); memcpy(e.s, tn.s, 12);
        entries.push_back(e);
        nameBlob += tn.name;
    }

    NodeFileHeader header;
    header.nodeCount      = (uint32_t)nodes.size();
    header.rootCount      = (uint32_t)roots.size();
    header.meshRangeCount = (uint32_t)meshRanges.size();

    // Payload: meshRanges | entries | roots | name blob
    std::vector<char> payload;
    auto append = [&](const void* d, size_t n) {
        const char* p = static_cast<const char*>(d);
        payload.insert(payload.end(), p, p + n);
    };
    append(meshRanges.data(), meshRanges.size() * sizeof(NodeFileMeshRange));
    append(entries.data(),    entries.size()    * sizeof(NodeFileEntry));
    append(roots.data(),      roots.size()      * sizeof(int32_t));
    append(nameBlob.data(),   nameBlob.size());

    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/nodes.meta";
    return ImporterUtils::SaveBuffer(path, header, payload);
}

bool SceneImporter::SaveSkinMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel) {
    if (gltfModel.skins.empty()) return true;

    SkinFileHeader hdr;
    hdr.skinCount = (uint32_t)gltfModel.skins.size();

    std::vector<char> payload;
    auto append = [&](const void* d, size_t n) {
        const char* p = static_cast<const char*>(d);
        payload.insert(payload.end(), p, p + n);
    };

    for (const auto& skin : gltfModel.skins) {
        uint32_t nameLen   = (uint32_t)skin.name.size();
        uint32_t jointCount = (uint32_t)skin.joints.size();
        append(&nameLen,    sizeof(uint32_t));
        append(&jointCount, sizeof(uint32_t));
        if (nameLen > 0)
            append(skin.name.data(), nameLen);

        for (int ji : skin.joints) {
            int32_t jIdx = ji;
            append(&jIdx, sizeof(int32_t));
        }

        // Inverse bind matrices — GLTF MAT4 is column-major; transpose to row-major for DX.
        std::vector<Matrix> ibms(jointCount, Matrix::Identity);
        if (skin.inverseBindMatrices >= 0 &&
            skin.inverseBindMatrices < (int)gltfModel.accessors.size()) {
            const auto& acc  = gltfModel.accessors[skin.inverseBindMatrices];
            const auto& view = gltfModel.bufferViews[acc.bufferView];
            const uint8_t* data = gltfModel.buffers[view.buffer].data.data()
                                  + view.byteOffset + acc.byteOffset;
            const size_t stride = view.byteStride ? view.byteStride : 64u;
            const uint32_t count = std::min((uint32_t)acc.count, jointCount);
            for (uint32_t j = 0; j < count; ++j) {
                memcpy(&ibms[j], data + j * stride, 64);
                ibms[j] = ibms[j].Transpose(); // GLTF column-major → DX row-major
            }
        }
        append(ibms.data(), jointCount * sizeof(Matrix));
    }

    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/skins.meta";
    return ImporterUtils::SaveBuffer(path, hdr, payload);
}

bool SceneImporter::LoadSkins(const std::string& sceneName, std::vector<SkinInfo>& outSkins) {
    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/skins.meta";
    SkinFileHeader hdr;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(path, hdr, raw)) return false;
    if (hdr.magic != 0x534B4E53 || hdr.version == 0) return false;

    const char* cur = raw.data() + sizeof(SkinFileHeader);
    const char* end = raw.data() + raw.size();

    outSkins.clear();
    outSkins.reserve(hdr.skinCount);
    for (uint32_t i = 0; i < hdr.skinCount; ++i) {
        if (cur + 8 > end) return false;
        uint32_t nameLen, jointCount;
        memcpy(&nameLen,    cur, 4); cur += 4;
        memcpy(&jointCount, cur, 4); cur += 4;

        SkinInfo si;
        if (nameLen > 0 && cur + nameLen <= end) {
            si.name = std::string(cur, nameLen);
            cur += nameLen;
        }

        if (jointCount > 0) {
            if (cur + jointCount * sizeof(int32_t) > end) return false;
            si.jointNodeIndices.resize(jointCount);
            memcpy(si.jointNodeIndices.data(), cur, jointCount * sizeof(int32_t));
            cur += jointCount * sizeof(int32_t);

            if (cur + jointCount * sizeof(Matrix) > end) return false;
            si.inverseBindMatrices.resize(jointCount);
            memcpy(si.inverseBindMatrices.data(), cur, jointCount * sizeof(Matrix));
            cur += jointCount * sizeof(Matrix);
        }

        outSkins.push_back(std::move(si));
    }
    return !outSkins.empty();
}

bool SceneImporter::LoadMaterialIndices(const std::string& sceneName, std::vector<int>& outMatIndices) {
    SceneHeader header;
    std::vector<char> raw;
    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";
    if (!ImporterUtils::LoadBuffer(path, header, raw)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x53434E45)) return false;

    size_t offset       = sizeof(SceneHeader);
    size_t expectedSize = header.meshCount * sizeof(int32_t);
    if (raw.size() < offset + expectedSize) return false;

    outMatIndices.resize(header.meshCount);
    memcpy(outMatIndices.data(), raw.data() + offset, expectedSize);
    return true;
}

bool SceneImporter::LoadNodeTree(const std::string& sceneName, std::vector<NodeInfo>& outNodes) {
    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/nodes.meta";
    NodeFileHeader header;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(path, header, raw)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x4E4F4445) || header.version > 2) return false;

    const char* cur = raw.data() + sizeof(NodeFileHeader);
    const char* end = raw.data() + raw.size();

    const bool isV2    = (header.version >= 2);
    const size_t entrySize = isV2 ? sizeof(NodeFileEntry) : sizeof(NodeFileEntryV1);

    size_t rangesBytes  = header.meshRangeCount * sizeof(NodeFileMeshRange);
    size_t entriesBytes = header.nodeCount      * entrySize;
    size_t rootsBytes   = header.rootCount      * sizeof(int32_t);
    if (cur + rangesBytes + entriesBytes + rootsBytes > end) return false;

    const auto* ranges  = reinterpret_cast<const NodeFileMeshRange*>(cur); cur += rangesBytes;
    const char* entryBase = cur;                                            cur += entriesBytes;
    cur += rootsBytes; // skip root list — not needed by callers
    const char* strCur  = cur;

    outNodes.clear();
    outNodes.reserve(header.nodeCount);
    for (uint32_t i = 0; i < header.nodeCount; ++i) {
        const char* ep = entryBase + i * entrySize;
        NodeInfo ni;

        int32_t  parentIndex, meshRangeIdx;
        int32_t  skinIndex = -1;
        uint32_t nameLen;
        const float* t;
        const float* r;
        const float* s;

        if (isV2) {
            const NodeFileEntry* e = reinterpret_cast<const NodeFileEntry*>(ep);
            parentIndex  = e->parentIndex;
            meshRangeIdx = e->meshRangeIdx;
            skinIndex    = e->skinIndex;
            nameLen      = e->nameLen;
            t = e->t; r = e->r; s = e->s;
        } else {
            const NodeFileEntryV1* e = reinterpret_cast<const NodeFileEntryV1*>(ep);
            parentIndex  = e->parentIndex;
            meshRangeIdx = e->meshRangeIdx;
            nameLen      = e->nameLen;
            t = e->t; r = e->r; s = e->s;
        }

        ni.parentIndex = parentIndex;
        ni.skinIndex   = skinIndex;
        if (meshRangeIdx >= 0 && meshRangeIdx < (int)header.meshRangeCount) {
            ni.meshFileStart = (int)ranges[meshRangeIdx].fileStart;
            ni.meshFileCount = (int)ranges[meshRangeIdx].fileCount;
        } else {
            ni.meshFileStart = -1;
            ni.meshFileCount = 0;
        }
        if (nameLen > 0 && strCur + nameLen <= end) {
            ni.name = std::string(strCur, nameLen);
            strCur += nameLen;
        }
        ni.translation = Vector3(t[0], t[1], t[2]);
        ni.rotation    = Quaternion(r[0], r[1], r[2], r[3]);
        ni.scale       = Vector3(s[0], s[1], s[2]);
        outNodes.push_back(std::move(ni));
    }
    return !outNodes.empty();
}