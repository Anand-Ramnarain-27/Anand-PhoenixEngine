// Real implementation of CreateNonAnimationResource() (declared in
// ModuleResources.h) - constructs Mesh/Material/Texture/Model resources.
// Engine/Player-only: touches GPU-facing code (ModuleStaticBuffer et al,
// transitively via these classes' own .cpp files). PhoenixCore links a
// nullptr-returning stub instead - see ModuleResourcesFactoriesStub.cpp.
#include "Globals.h"
#include "ModuleResources.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ResourceTexture.h"
#include "ResourceModel.h"

ResourceBase* CreateNonAnimationResource(ResourceBase::Type type, UID uid,
    const std::string& libraryPath, UID textureUID, const std::string& assetPath){
    switch (type){
    case ResourceBase::Type::Mesh: {
        auto* r = new ResourceMesh(uid);
        r->libraryFile = libraryPath; r->assetsFile = assetPath;
        return r;
    }
    case ResourceBase::Type::Material: {
        auto* r = new ResourceMaterial(uid);
        r->libraryFile = libraryPath; r->assetsFile = assetPath; r->textureUID = textureUID;
        return r;
    }
    case ResourceBase::Type::Texture: {
        auto* r = new ResourceTexture(uid);
        r->libraryFile = libraryPath; r->assetsFile = assetPath;
        return r;
    }
    case ResourceBase::Type::Model: {
        auto* r = new ResourceModel(uid);
        r->libraryFile = libraryPath; r->assetsFile = assetPath;
        return r;
    }
    default:
        return nullptr;
    }
}
