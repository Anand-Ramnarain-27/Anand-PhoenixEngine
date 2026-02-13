#include "Globals.h"
#include "TextureImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include <DirectXTex.h>
#include <filesystem>
#include <cstring>

using namespace DirectX;

bool TextureImporter::Import(const char* sourcePath, const std::string& outputPath)
{
    LOG("TextureImporter: Importing texture from %s", sourcePath);

    ScratchImage image;
    HRESULT hr = S_OK;

    std::wstring wSourcePath = std::filesystem::path(sourcePath).wstring();
    std::wstring extension = std::filesystem::path(sourcePath).extension().wstring();

    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

    if (extension == L".dds")
    {
        hr = LoadFromDDSFile(wSourcePath.c_str(), DDS_FLAGS_NONE, nullptr, image);
    }
    else if (extension == L".tga")
    {
        hr = LoadFromTGAFile(wSourcePath.c_str(), nullptr, image);
    }
    else if (extension == L".hdr")
    {
        hr = LoadFromHDRFile(wSourcePath.c_str(), nullptr, image);
    }
    else // PNG, JPG, BMP, etc.
    {
        hr = LoadFromWICFile(wSourcePath.c_str(), WIC_FLAGS_NONE, nullptr, image);
    }

    if (FAILED(hr))
    {
        LOG("TextureImporter: Failed to load source image: HRESULT 0x%08X", hr);
        return false;
    }

    ScratchImage mipChain;
    if (image.GetMetadata().mipLevels == 1)
    {
        hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);

        if (FAILED(hr))
        {
            LOG("TextureImporter: Failed to generate mipmaps, using original");
            mipChain = std::move(image);
        }
    }
    else
    {
        mipChain = std::move(image);
    }

    // Compress to BC format for better GPU performance
    // For now, we'll save as-is
    ScratchImage compressed;
    const TexMetadata& metadata = mipChain.GetMetadata();
    if (!IsCompressed(metadata.format))
    {
        DXGI_FORMAT compressedFormat = DXGI_FORMAT_BC1_UNORM; 

        if (HasAlpha(metadata.format))
        {
            compressedFormat = DXGI_FORMAT_BC3_UNORM; 
        }

        hr = Compress(mipChain.GetImages(), mipChain.GetImageCount(), metadata, compressedFormat, TEX_COMPRESS_DEFAULT, 1.0f, compressed);

        if (FAILED(hr))
        {
            LOG("TextureImporter: Compression failed, saving uncompressed");
            compressed = std::move(mipChain);
        }
    }
    else
    {
        compressed = std::move(mipChain);
    }

    std::wstring wOutputPath = std::filesystem::path(outputPath).wstring();
    hr = SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DDS_FLAGS_NONE, wOutputPath.c_str());

    if (FAILED(hr))
    {
        LOG("TextureImporter: Failed to save DDS file: HRESULT 0x%08X", hr);
        return false;
    }

    const TexMetadata& finalMeta = compressed.GetMetadata();
    if (!SaveMetadata(outputPath, (uint32_t)finalMeta.width, (uint32_t)finalMeta.height, (uint32_t)finalMeta.mipLevels, (uint32_t)finalMeta.format))
    {
        LOG("TextureImporter: Warning - Failed to save texture metadata");
    }

    LOG("TextureImporter: Successfully imported texture to %s", outputPath.c_str());
    LOG("  Resolution: %dx%d, Mips: %d, Format: %d", (int)finalMeta.width, (int)finalMeta.height, (int)finalMeta.mipLevels, (int)finalMeta.format);

    return true;
}

bool TextureImporter::Load(const std::string& file, ComPtr<ID3D12Resource>& outTexture)
{
    // This would integrate with your ModuleResources to load the DDS file
    // For now, this is a placeholder showing the structure

    LOG("TextureImporter: Loading texture from %s", file.c_str());

    std::string metaPath = file + ".meta";

    char* buffer = nullptr;
    uint32_t fileSize = app->getFileSystem()->Load(metaPath.c_str(), &buffer);

    if (buffer && fileSize >= sizeof(TextureHeader))
    {
        TextureHeader header;
        memcpy(&header, buffer, sizeof(TextureHeader));
        delete[] buffer;

        LOG("  Texture info: %dx%d, %d mips, format %d",
            header.width, header.height, header.mipLevels, header.format);
    }

    // The actual DDS loading would be done through DirectXTex and D3D12
    // This integrates with your existing ModuleResources::createTextureFromFile

    return true;
}

std::string TextureImporter::GetTextureName(const char* filePath)
{
    namespace fs = std::filesystem;
    return fs::path(filePath).stem().string();
}

bool TextureImporter::SaveMetadata(const std::string& ddsPath, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t format)
{
    ModuleFileSystem* fs = app->getFileSystem();

    TextureHeader header;
    header.width = width;
    header.height = height;
    header.mipLevels = mipLevels;
    header.format = format;

    std::string metaPath = ddsPath + ".meta";

    return fs->Save(metaPath.c_str(), &header, sizeof(TextureHeader));
}