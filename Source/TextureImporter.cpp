#include "Globals.h"
#include "TextureImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleD3D12.h"
#include "ShaderTableDesc.h"
#include <DirectXTex.h>
#include <filesystem>

using namespace DirectX;

bool TextureImporter::Import(const char* sourcePath, const std::string& outputPath)
{
    std::wstring wSource = std::filesystem::path(sourcePath).wstring();
    std::wstring ext = std::filesystem::path(sourcePath).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    ScratchImage image;
    HRESULT hr;

    if (ext == L".dds") hr = LoadFromDDSFile(wSource.c_str(), DDS_FLAGS_NONE, nullptr, image);
    else if (ext == L".tga") hr = LoadFromTGAFile(wSource.c_str(), nullptr, image);
    else if (ext == L".hdr") hr = LoadFromHDRFile(wSource.c_str(), nullptr, image);
    else                     hr = LoadFromWICFile(wSource.c_str(), WIC_FLAGS_NONE, nullptr, image);

    if (FAILED(hr))
    {
        LOG("TextureImporter: Failed to load image 0x%08X", hr);
        return false;
    }

    ScratchImage mipChain;
    if (image.GetMetadata().mipLevels == 1)
    {
        hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
        if (FAILED(hr)) mipChain = std::move(image);
    }
    else
    {
        mipChain = std::move(image);
    }

    ScratchImage compressed;
    const TexMetadata& meta = mipChain.GetMetadata();

    if (!IsCompressed(meta.format))
    {
        DXGI_FORMAT fmt = HasAlpha(meta.format) ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
        hr = Compress(mipChain.GetImages(), mipChain.GetImageCount(), meta, fmt, TEX_COMPRESS_DEFAULT, 1.0f, compressed);
        if (FAILED(hr)) compressed = std::move(mipChain);
    }
    else
    {
        compressed = std::move(mipChain);
    }

    std::wstring wOutput = std::filesystem::path(outputPath).wstring();
    hr = SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DDS_FLAGS_NONE, wOutput.c_str());

    if (FAILED(hr))
    {
        LOG("TextureImporter: Failed to save DDS 0x%08X", hr);
        return false;
    }

    const TexMetadata& finalMeta = compressed.GetMetadata();
    SaveMetadata(outputPath, (uint32_t)finalMeta.width, (uint32_t)finalMeta.height, (uint32_t)finalMeta.mipLevels, (uint32_t)finalMeta.format);

    LOG("TextureImporter: Imported %s (%dx%d, %d mips)", outputPath.c_str(), (int)finalMeta.width, (int)finalMeta.height, (int)finalMeta.mipLevels);
    return true;
}

bool TextureImporter::Load(const std::string& file, ComPtr<ID3D12Resource>& outTexture, D3D12_GPU_DESCRIPTOR_HANDLE& outSRV)
{
    char* buffer = nullptr;
    uint32_t fileSize = app->getFileSystem()->Load((file + ".meta").c_str(), &buffer);

    if (!buffer || fileSize < sizeof(TextureHeader))
    {
        delete[] buffer;
        LOG("TextureImporter: Missing or invalid metadata for %s", file.c_str());
        return false;
    }

    TextureHeader header;
    memcpy(&header, buffer, sizeof(header));
    delete[] buffer;

    if (header.magic != 0x54455854 || header.version != 1)
    {
        LOG("TextureImporter: Bad metadata magic/version");
        return false;
    }

    outTexture = app->getResources()->createTextureFromFile(file, true);
    if (!outTexture)
    {
        LOG("TextureImporter: Failed to create texture resource");
        return false;
    }

    ShaderTableDesc tableDesc = app->getShaderDescriptors()->allocTable();
    tableDesc.createTexture2DSRV(outTexture.Get(), 0, (DXGI_FORMAT)header.format, header.mipLevels);
    outSRV = tableDesc.getGPUHandle(0);

    return true;
}

std::string TextureImporter::GetTextureName(const char* filePath)
{
    return std::filesystem::path(filePath).stem().string();
}

bool TextureImporter::SaveMetadata(const std::string& ddsPath, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t format)
{
    TextureHeader header;
    header.width = width;
    header.height = height;
    header.mipLevels = mipLevels;
    header.format = format;
    return app->getFileSystem()->Save((ddsPath + ".meta").c_str(), &header, sizeof(header));
}