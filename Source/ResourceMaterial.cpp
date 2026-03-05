#include "Globals.h"
#include "ResourceMaterial.h"
#include "MaterialImporter.h"
#include "ResourceTexture.h"
#include "Application.h"
#include "ModuleResources.h"

ResourceMaterial::ResourceMaterial(UID uid) : ResourceBase(uid, Type::Material) {}
ResourceMaterial::~ResourceMaterial() { UnloadFromMemory(); }

bool ResourceMaterial::LoadInMemory() {
    if (m_material) return true;
    std::unique_ptr<Material> mat;
    if (!MaterialImporter::Load(libraryFile, mat)) { LOG("ResourceMaterial: Failed to load %s", libraryFile.c_str()); m_material = std::make_unique<Material>(); return true; }
    m_material = std::move(mat);
    if (textureUID != 0) {
        auto* texRes = static_cast<ResourceTexture*>(app->getResources()->RequestResource(textureUID));
        if (texRes && texRes->hasTexture()) m_material->setBaseColorTexture(ComPtr<ID3D12Resource>(texRes->getResource()), texRes->getSRV());
    }
    return true;
}

void ResourceMaterial::UnloadFromMemory() {
    if (textureUID != 0) {
        auto& loaded = app->getResources()->getLoadedResources();
        auto it = loaded.find(textureUID);
        if (it != loaded.end()) app->getResources()->ReleaseResource(static_cast<ResourceTexture*>(it->second));
    }
    m_material.reset();
}