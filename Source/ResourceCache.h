#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class Model;
class Material;
class Mesh;

class ResourceCache
{
public:
    ResourceCache() = default;
    ~ResourceCache() = default;

    std::shared_ptr<Model> getOrLoadModel(const std::string& path);
    std::shared_ptr<Mesh> getOrLoadMesh(const std::string& path);
    std::shared_ptr<Material> getOrLoadMaterial(const std::string& path);

    void clear();
    void clearUnused();
    void getStats(int& modelCount, int& meshCount, int& materialCount) const;

private:
    std::unordered_map<std::string, std::shared_ptr<Model>> m_modelCache;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_meshCache;
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materialCache;
};