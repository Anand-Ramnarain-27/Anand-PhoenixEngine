#pragma once
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

class ComponentMesh : public Component {
public:
    explicit ComponentMesh(GameObject* owner);
    ~ComponentMesh() override;

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
    std::vector<MeshEntry>& getEntries() { return m_entries; }
    const std::vector<MeshEntry>& getEntries() const { return m_entries; }

    void computeLocalAABB();
    bool hasAABB() const { return m_hasAABB; }
    const Vector3& getLocalAABBMin() const { return m_localAABBMin; }
    const Vector3& getLocalAABBMax() const { return m_localAABBMax; }
    void getWorldAABB(Vector3& outMin, Vector3& outMax) const;

    // Frustum-culling visibility flag, recomputed once per frame in
    // ModuleEditor::preRender() against the active game camera's frustum.
    //
    // We deliberately FLAG visibility instead of deactivating the GameObject or
    // removing it from the scene graph. Deactivation would also stop physics,
    // animation, AI and script updates for objects that are merely off-screen,
    // which is incorrect (e.g. an off-screen enemy should keep moving/thinking).
    // Visibility flagging lets the render loop skip just the draw call for this
    // mesh while every other system continues to update it normally.
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool v) { m_isVisible = v; }

    // Skinning data — call once when spawning a skinned GLTF model.
    // skin is copied into this component so ResourceModel can be released.
    void setSkinData(const ResourceModel::Skin& skin, std::vector<GameObject*> joints);
    bool hasSkinData() const { return m_hasSkin; }
    const ResourceModel::Skin& getLocalSkin() const { return m_localSkin; }
    const std::vector<GameObject*>& getSkinJoints() const { return m_skinJoints; }

    // Called by the editor before destroying a GameObject to prevent dangling joint pointers.
    void nullifyJoint(const GameObject* go){
        for (auto& j : m_skinJoints) if (j == go) j = nullptr;
    }

    // Morph target weights — one float per target, written by the animation system each frame.
    // Shared across all mesh primitives in this component (index 0 = first target of the mesh).
    static constexpr int MAX_MORPH_WEIGHTS = 64;
    void setMorphWeight(int index, float weight);
    const float* getMorphWeights() const { return m_morphWeights; }
    bool getMorphWeightsDirty() const { return m_morphWeightsDirty; }
    void clearMorphWeightsDirty() { m_morphWeightsDirty = false; }

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

    bool m_hasSkin = false;
    ResourceModel::Skin m_localSkin; // owned copy of the skin definition
    std::vector<GameObject*> m_skinJoints; // joint GameObjects in joint-index order (not owned)

    float m_morphWeights[MAX_MORPH_WEIGHTS] = {};
    bool m_morphWeightsDirty = false;

    bool m_drawBindPose = false;
};
