#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class Model;
class Material;
class Mesh;

// Resource Cache - prevents duplicate loading of assets
// Implements reference counting and lifetime management
class ResourceCache
{
public:
    ResourceCache() = default;
    ~ResourceCache() = default;

    // Get or load a model (returns cached instance if already loaded)
    std::shared_ptr<Model> getOrLoadModel(const std::string& path);

    // Get or load a mesh
    std::shared_ptr<Mesh> getOrLoadMesh(const std::string& path);

    // Get or load a material
    std::shared_ptr<Material> getOrLoadMaterial(const std::string& path);

    // Clear all cached resources
    void clear();

    // Clear unused resources (ref count == 1, meaning only cache holds reference)
    void clearUnused();

    // Get cache statistics
    void getStats(int& modelCount, int& meshCount, int& materialCount) const;

private:
    std::unordered_map<std::string, std::shared_ptr<Model>> m_modelCache;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_meshCache;
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materialCache;
};