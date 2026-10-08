#pragma once
// Texture lookup shared by the effect passes (billboards, trails, particles, decals).

#include "Application.h"
#include "ModuleGPUResources.h"
#include <d3d12.h>
#include <wrl.h>
#include <cctype>
#include <filesystem>
#include <string>

// Shared texture lookup for the effect passes (BillboardPass, TrailPass, ParticlePass, DecalPass). The asset picker
// stores the source path (e.g. "Assets/VFX/Textures/spark.png"); when that can't be loaded directly, the imported
// copy at "Library/Textures/<stem>.dds" is tried. Returns null when neither loads; outResolved gets the path used.
inline Microsoft::WRL::ComPtr<ID3D12Resource> loadEffectTexture(const std::string& path, std::string* outResolved = nullptr){
    Microsoft::WRL::ComPtr<ID3D12Resource> tex;
    if (path.empty()) return tex;
    auto* gpu = app ? app->getGPUResources() : nullptr;
    if (!gpu) return tex;

    tex = gpu->createTextureFromFile(path, true);
    if (tex){
        if (outResolved) *outResolved = path;
        return tex;
    }

    namespace fs = std::filesystem;
    fs::path fp(path);
    std::string ext = fp.extension().string();
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext != ".dds"){
        const std::string ddsCandidate = "Library/Textures/" + fp.stem().string() + ".dds";
        tex = gpu->createTextureFromFile(ddsCandidate, true);
        if (tex && outResolved) *outResolved = ddsCandidate;
    }
    return tex;
}
