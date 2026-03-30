#pragma once
#include "Component.h"
#include "MeshEntry.h"
#include "ResourceSkin.h"
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
    void setProceduralModel(std::unique_ptr<Model> model);
    void overrideMaterial(int slot, UID materialUID);
    void rebuildMaterialBuffers();
    void flushDeferredReleases();
    void markMaterialsDirty();

    void render(ID3D12GraphicsCommandList* cmd) override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Mesh; }

    Model* getProceduralModel() const { return m_proceduralModel.get(); }
    const std::string& getModelPath() const { return m_modelPath; }
    UID getModelUID() const { return m_modelUID; }
    const std::vector<MeshEntry>& getEntries() const { return m_entries; }

    void computeLocalAABB();
    bool hasAABB() const { return m_hasAABB; }
    const Vector3& getLocalAABBMin() const { return m_localAABBMin; }
    const Vector3& getLocalAABBMax() const { return m_localAABBMax; }
    void getWorldAABB(Vector3& outMin, Vector3& outMax) const;

    D3D12_GPU_VIRTUAL_ADDRESS getBindPoseVA() const;
    D3D12_GPU_VIRTUAL_ADDRESS getSkinWeightsVA() const;
    D3D12_GPU_VIRTUAL_ADDRESS getMorphVertsVA() const;
    uint32_t getMorphWeightCount() const;
    void setMorphWeights(const std::vector<float>& w);

    bool isSkinned() const;
    ResourceSkin* getSkinResource() const;
    uint32_t getTotalVertexCount() const;
    const std::vector<float>& getMorphWeightsVec() const;
private:
    void releaseEntries();
    void rebuildEntry(MeshEntry& e);

    ResourceSkin* m_skin = nullptr;

    UID m_modelUID = 0;
    std::string m_modelPath;
    std::vector<MeshEntry> m_entries;
    std::shared_ptr<Model> m_proceduralModel;
    std::vector<ComPtr<ID3D12Resource>> m_proceduralMaterialBuffers;
    std::vector<ComPtr<ID3D12Resource>> m_deferredRelease;
    Vector3 m_localAABBMin = {};
    Vector3 m_localAABBMax = {};
    bool m_hasAABB = false;
    bool m_materialsDirty = false;
    std::vector<float> m_morphWeights;
    bool m_morphDirty = false;
};