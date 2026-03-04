#include "Globals.h"
#include "ResourceTexture.h"
#include "TextureImporter.h"

ResourceTexture::ResourceTexture(UID uid)
    : ResourceBase(uid, Type::Texture)
{
}

ResourceTexture::~ResourceTexture()
{
    UnloadFromMemory();
}

bool ResourceTexture::LoadInMemory()
{
    if (m_texture) return true;

    ComPtr<ID3D12Resource>      tex;
    D3D12_GPU_DESCRIPTOR_HANDLE srv{};

    if (!TextureImporter::Load(libraryFile, tex, srv))
    {
        LOG("ResourceTexture: Failed to load %s", libraryFile.c_str());
        return false;
    }

    m_texture = tex;
    m_srv = srv;
    return true;
}

void ResourceTexture::UnloadFromMemory()
{
    m_texture.Reset();
    m_srv = {};
}