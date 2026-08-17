#pragma once
#include "ShaderTableDesc.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;

class RenderTexture;

struct PostProcessEffectDef {
    enum class Domain { PreTonemap, PostGamma };

    std::string name;
    std::string shaderFile;
    Domain domain = Domain::PreTonemap;
    int order = 0;
    bool enabled = true;
    float params[8] = {};
};

struct PostProcessEffect {
    PostProcessEffectDef def;
    ComPtr<ID3D12PipelineState> pso;
};

class PostProcessChain {
public:
    bool init(ID3D12Device* device);
    void reload(ID3D12Device* device);

    int countEnabled(PostProcessEffectDef::Domain domain) const;

    RenderTexture* run(ID3D12GraphicsCommandList* cmd, PostProcessEffectDef::Domain domain,
                       RenderTexture* a, RenderTexture* b);

    std::vector<PostProcessEffect>& getEffects() { return m_effects; }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createFallbackAux(ID3D12Device* device);
    ComPtr<ID3D12PipelineState> buildPSO(ID3D12Device* device, const PostProcessEffectDef& def);
    void seedDefaultManifests(const std::string& dir);
    void drawEffect(ID3D12GraphicsCommandList* cmd, const PostProcessEffect& effect,
                    RenderTexture* input, RenderTexture* output);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ShaderTableDesc m_fallbackAuxSRV;
    std::vector<PostProcessEffect> m_effects;
};
