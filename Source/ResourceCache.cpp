#include "Globals.h"
#include "ResourceCache.h"
#include "Model.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"

std::shared_ptr<Model> ResourceCache::getOrLoadModel(const std::string& path)
{
    // Check if already cached
    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end())
    {
        LOG("ResourceCache: Returning cached model: %s", path.c_str());
        return it->second;
    }

    // Load new model
    LOG("ResourceCache: Loading new model: %s", path.c_str());
    auto model = std::make_shared<Model>();

    if (!model->load(path.c_str()))
    {
        LOG("ResourceCache: Failed to load model: %s", path.c_str());
        return nullptr;
    }

    // Cache it
    m_modelCache[path] = model;
    return model;
}

std::shared_ptr<Mesh> ResourceCache::getOrLoadMesh(const std::string& path)
{
    // Check if already cached
    auto it = m_meshCache.find(path);
    if (it != m_meshCache.end())
    {
        LOG("ResourceCache: Returning cached mesh: %s", path.c_str());
        return it->second;
    }

    // Load new mesh
    LOG("ResourceCache: Loading new mesh: %s", path.c_str());
    std::unique_ptr<Mesh> meshPtr;

    if (!MeshImporter::Load(path, meshPtr))
    {
        LOG("ResourceCache: Failed to load mesh: %s", path.c_str());
        return nullptr;
    }

    // Convert unique_ptr to shared_ptr
    auto mesh = std::shared_ptr<Mesh>(meshPtr.release());

    // Cache it
    m_meshCache[path] = mesh;
    return mesh;
}

std::shared_ptr<Material> ResourceCache::getOrLoadMaterial(const std::string& path)
{
    // Check if already cached
    auto it = m_materialCache.find(path);
    if (it != m_materialCache.end())
    {
        LOG("ResourceCache: Returning cached material: %s", path.c_str());
        return it->second;
    }

    // Load new material
    LOG("ResourceCache: Loading new material: %s", path.c_str());
    std::unique_ptr<Material> matPtr;

    if (!MaterialImporter::Load(path, matPtr))
    {
        LOG("ResourceCache: Failed to load material: %s", path.c_str());
        return nullptr;
    }

    // Convert unique_ptr to shared_ptr
    auto material = std::shared_ptr<Material>(matPtr.release());

    // Cache it
    m_materialCache[path] = material;
    return material;
}

void ResourceCache::clear()
{
    LOG("ResourceCache: Clearing all cached resources");
    m_modelCache.clear();
    m_meshCache.clear();
    m_materialCache.clear();
}

void ResourceCache::clearUnused()
{
    // Clear models with only 1 reference (cache is the only holder)
    for (auto it = m_modelCache.begin(); it != m_modelCache.end();)
    {
        if (it->second.use_count() == 1)
        {
            LOG("ResourceCache: Removing unused model: %s", it->first.c_str());
            it = m_modelCache.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Clear meshes with only 1 reference
    for (auto it = m_meshCache.begin(); it != m_meshCache.end();)
    {
        if (it->second.use_count() == 1)
        {
            LOG("ResourceCache: Removing unused mesh: %s", it->first.c_str());
            it = m_meshCache.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Clear materials with only 1 reference
    for (auto it = m_materialCache.begin(); it != m_materialCache.end();)
    {
        if (it->second.use_count() == 1)
        {
            LOG("ResourceCache: Removing unused material: %s", it->first.c_str());
            it = m_materialCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ResourceCache::getStats(int& modelCount, int& meshCount, int& materialCount) const
{
    modelCount = (int)m_modelCache.size();
    meshCount = (int)m_meshCache.size();
    materialCount = (int)m_materialCache.size();
}