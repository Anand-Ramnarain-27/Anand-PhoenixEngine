#pragma once

#include "Globals.h"
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class TextureImporter
{
public:
    struct TextureHeader
    {
        uint32_t magic = 0x54455854; 
        uint32_t version = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 0;
        uint32_t format = 0;
    };

public:
    static bool Import(const char* sourcePath, const std::string& outputPath);
    static bool Load(const std::string& file, ComPtr<ID3D12Resource>& outTexture, D3D12_GPU_DESCRIPTOR_HANDLE& outSRV);

    static std::string GetTextureName(const char* filePath);

private:
    static bool SaveMetadata(const std::string& ddsPath, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t format);
};