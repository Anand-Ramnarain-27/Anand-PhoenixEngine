#pragma once
// Renderable mesh: imported model meshes and materials, LODs, skinning and morph targets.

#include "Component.h"
#include "MeshEntry.h"
#include "ResourceModel.h"
#include <vector>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class ResourceMesh;
class ResourceMaterial;
class Model;
class Material;

/// One or more mesh + material entries (MeshEntry) for this GameObject, from a model asset or built
/// procedurally. Skinned meshes bind their joints by name once the whole scene is loaded (resolveDeferredSkin).
class ComponentMesh : public Component {
public:
    explicit ComponentMesh(GameObject* owner);
    ~ComponentMesh() override;

    /// Loads every mesh of an imported model asset.
    bool loadModel(const char* filePath);
    bool loadMeshSubset(const std::string& assetPath, int startMesh, int meshCount);
    void addMeshEntry(UID meshUID, UID materialUID);
    void setProceduralModel(std::unique_ptr<Model> model);
    void overrideMaterial(int slot, UID materialUID);
    void rebuildMaterialBuffers();
    void flushDeferredReleases();
    void markMaterialsDirty();

    void render(ID3D12GraphicsCommandList* cmd) override;
    void onEditor() override;
    void onDrawGizmos() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Mesh; }

    Model* getProceduralModel() const { return m_proceduralModel.get(); }
    const std::string& getModelPath() const { return m_modelPath; }
    UID getModelUID() const { return m_modelUID; }
    std::vector<MeshEntry>& getEntries(){ return m_entries; }
    const std::vector<MeshEntry>& getEntries() const { return m_entries; }

    void computeLocalAABB();
    bool hasAABB() const { return m_hasAABB; }
    const Vector3& getLocalAABBMin() const { return m_localAABBMin; }
    const Vector3& getLocalAABBMax() const { return m_localAABBMax; }
    void getWorldAABB(Vector3& outMin, Vector3& outMax) const;

    /// Ray tests against the mesh's actual triangles (both faces), for collision that needs more than the
    /// bounding box - stairs, doorways, low props. Skinned meshes have none to offer (their CPU vertices are
    /// the bind pose), nor do meshes without CPU-side geometry; callers fall back to the bounds for those.
    bool hasRaycastTriangles() const;
    // World-space ray, `dir` normalized. On a hit within maxDist: true, distance and world-space normal
    // (facing against the ray).
    bool raycastTriangles(const Vector3& origin, const Vector3& dir, float maxDist, float& outDist, Vector3& outNormal) const;

    bool isVisible() const { return m_isVisible; }
    void setVisible(bool v){ m_isVisible = v; }

    /// Runtime-only VFX overrides for every entry of this mesh (not saved). Set through Phoenix::VFX.
    MeshVfxParams vfx;

    void setSkinData(const ResourceModel::Skin& skin, std::vector<GameObject*> joints);

    /// Binds joint names stored by onLoad to the scene's GameObjects; call once the hierarchy is complete.
    void resolveDeferredSkin();

    bool hasSkinData() const { return m_hasSkin; }
    const ResourceModel::Skin& getLocalSkin() const { return m_localSkin; }
    const std::vector<GameObject*>& getSkinJoints() const { return m_skinJoints; }

    void nullifyJoint(const GameObject* go){
        for (auto& j : m_skinJoints) if (j == go) j = nullptr;
    }

    struct LODLevel {
        UID meshUID = 0;
        float screenCoverageThreshold = 0.0f;
    };
    void setLODLevels(std::vector<LODLevel> levels);
    const std::vector<LODLevel>& getLODLevels() const { return m_lodLevels; }
    bool hasLODLevels() const { return !m_lodLevels.empty(); }

    /// Picks the LOD for `coverage` (fraction of the screen covered); forceIndex >= 0 overrides it.
    void updateLOD(float coverage, int forceIndex);
    int getCurrentLODIndex() const { return m_currentLOD; }
    float getLastScreenCoverage() const { return m_lastScreenCoverage; }

    static constexpr int MAX_MORPH_WEIGHTS = 64;
    void setMorphWeight(int index, float weight);
    const float* getMorphWeights() const { return m_morphWeights; }
    bool getMorphWeightsDirty() const { return m_morphWeightsDirty; }
    void clearMorphWeightsDirty(){ m_morphWeightsDirty = false; }

private:
    void releaseEntries();
    void rebuildEntry(MeshEntry& e);

    UID m_modelUID = 0;
    std::string m_modelPath;
    int m_meshFileStart = -1;
    int m_meshFileCount = 0;
    std::vector<MeshEntry> m_entries;
    std::shared_ptr<Model> m_proceduralModel;
    std::vector<ComPtr<ID3D12Resource>> m_proceduralMaterialBuffers;
    std::vector<ComPtr<ID3D12Resource>> m_deferredRelease;
    Vector3 m_localAABBMin = {};
    Vector3 m_localAABBMax = {};
    bool m_hasAABB = false;
    bool m_materialsDirty = false;
    bool m_isVisible = true;

    std::vector<LODLevel> m_lodLevels;
    int m_currentLOD = 0;
    float m_lastScreenCoverage = 1.0f;

    bool m_hasSkin = false;
    ResourceModel::Skin m_localSkin;
    std::vector<GameObject*> m_skinJoints;

    bool m_hasPendingSkin = false;
    ResourceModel::Skin m_pendingSkin;
    std::vector<std::string> m_pendingJointNames;

    float m_morphWeights[MAX_MORPH_WEIGHTS] = {};
    bool m_morphWeightsDirty = false;

    bool m_drawBindPose = false;
};
