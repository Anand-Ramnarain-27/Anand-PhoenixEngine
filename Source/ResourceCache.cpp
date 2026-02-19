#include "Globals.h"
#include "ResourceCache.h"
#include "Model.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"

template<typename T, typename LoadFn>
static std::shared_ptr<T> getOrLoad(
    std::unordered_map<std::string, std::shared_ptr<T>>& cache,
    const std::string& path, LoadFn load)
{
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    auto ptr = load(path);
    if (ptr) cache[path] = ptr;
    return ptr;
}

std::shared_ptr<Model> ResourceCache::getOrLoadModel(const std::string& path)
{
    return getOrLoad(m_modelCache, path, [](const std::string& p) -> std::shared_ptr<Model>
        {
            auto m = std::make_shared<Model>();
            if (!m->load(p.c_str())) { LOG("ResourceCache: Failed to load model: %s", p.c_str()); return {}; }
            return m;
        });
}

std::shared_ptr<Mesh> ResourceCache::getOrLoadMesh(const std::string& path)
{
    return getOrLoad(m_meshCache, path, [](const std::string& p) -> std::shared_ptr<Mesh>
        {
            std::unique_ptr<Mesh> ptr;
            if (!MeshImporter::Load(p, ptr)) { LOG("ResourceCache: Failed to load mesh: %s", p.c_str()); return {}; }
            return std::shared_ptr<Mesh>(ptr.release());
        });
}

std::shared_ptr<Material> ResourceCache::getOrLoadMaterial(const std::string& path)
{
    return getOrLoad(m_materialCache, path, [](const std::string& p) -> std::shared_ptr<Material>
        {
            std::unique_ptr<Material> ptr;
            if (!MaterialImporter::Load(p, ptr)) { LOG("ResourceCache: Failed to load material: %s", p.c_str()); return {}; }
            return std::shared_ptr<Material>(ptr.release());
        });
}

void ResourceCache::clear()
{
    m_modelCache.clear();
    m_meshCache.clear();
    m_materialCache.clear();
}

void ResourceCache::clearUnused()
{
    auto evict = [](auto& cache)
        {
            for (auto it = cache.begin(); it != cache.end();)
                it = (it->second.use_count() == 1) ? cache.erase(it) : std::next(it);
        };
    evict(m_modelCache);
    evict(m_meshCache);
    evict(m_materialCache);
}

void ResourceCache::getStats(int& modelCount, int& meshCount, int& materialCount) const
{
    modelCount = (int)m_modelCache.size();
    meshCount = (int)m_meshCache.size();
    materialCount = (int)m_materialCache.size();
}