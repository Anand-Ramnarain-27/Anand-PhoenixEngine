#include "Globals.h"
#include "PostProcessChain.h"
#include "Application.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleFileSystem.h"
#include "RenderTexture.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <algorithm>

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

namespace {
    const char* kPostProcessSubdir = "PostProcess/";

    std::wstring toWide(const std::string& s){
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
        return out;
    }

    const char* domainToString(PostProcessEffectDef::Domain d){
        return d == PostProcessEffectDef::Domain::PreTonemap ? "PreTonemap" : "PostGamma";
    }

    PostProcessEffectDef::Domain domainFromString(const std::string& s){
        return s == "PostGamma" ? PostProcessEffectDef::Domain::PostGamma : PostProcessEffectDef::Domain::PreTonemap;
    }
}

bool PostProcessChain::init(ID3D12Device* device){
    if (!createRootSignature(device)){
        LOG("PostProcessChain: root signature creation failed");
        return false;
    }
    if (!createFallbackAux(device)){
        LOG("PostProcessChain: fallback aux texture creation failed");
        return false;
    }
    reload(device);
    LOG("PostProcessChain: init OK (%zu plugin effects loaded)", m_effects.size());
    return true;
}

bool PostProcessChain::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE inputRange;
    inputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE auxRange;
    auxRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[0].InitAsConstants(8, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &inputRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[2].InitAsDescriptorTable(1, &auxRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
        LOG("PostProcessChain: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));
}

bool PostProcessChain::createFallbackAux(ID3D12Device* device){
    uint8_t black[4] = { 0, 0, 0, 0 };
    ComPtr<ID3D12Resource> tex = app->getGPUResources()->createRawTexture2D(black, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!tex) return false;

    m_fallbackAuxSRV = app->getShaderDescriptors()->allocTable("PostProcessFallbackAux");
    if (!m_fallbackAuxSRV.isValid()) return false;
    m_fallbackAuxSRV.createTexture2DSRV(tex.Get(), 0);

    app->getGPUResources()->deferRelease(tex);
    return true;
}

ComPtr<ID3D12PipelineState> PostProcessChain::buildPSO(ID3D12Device* device, const PostProcessEffectDef& def){
    auto vs = DX::ReadData(L"FullScreenVS.cso");

    const std::string localPath = app->getFileSystem()->GetAssetsPath() + kPostProcessSubdir + def.shaderFile;
    std::vector<uint8_t> ps;
    try {
        ps = DX::FileExists(toWide(localPath).c_str()) ? DX::ReadData(toWide(localPath).c_str())
                                                         : DX::ReadData(toWide(def.shaderFile).c_str());
    } catch (const std::exception&){
        LOG("PostProcessChain: failed to load shader '%s' for effect '%s'", def.shaderFile.c_str(), def.name.c_str());
        return nullptr;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { nullptr, 0 };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RTVFormats[0] = (def.domain == PostProcessEffectDef::Domain::PreTonemap)
                            ? kSceneColorFormat : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.NumRenderTargets = 1;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)))){
        LOG("PostProcessChain: PSO creation failed for effect '%s'", def.name.c_str());
        return nullptr;
    }
    return pso;
}

void PostProcessChain::seedDefaultManifests(const std::string& dir){
    auto write = [&](const PostProcessEffectDef& def){
        Document doc; doc.SetObject();
        auto& a = doc.GetAllocator();
        doc.AddMember("name", Value(def.name.c_str(), a), a);
        doc.AddMember("shader", Value(def.shaderFile.c_str(), a), a);
        doc.AddMember("domain", Value(domainToString(def.domain), a), a);
        doc.AddMember("order", def.order, a);
        doc.AddMember("enabled", def.enabled, a);
        Value params(kArrayType);
        for (float p : def.params) params.PushBack(p, a);
        doc.AddMember("params", params, a);

        StringBuffer sb;
        PrettyWriter<StringBuffer> writer(sb);
        doc.Accept(writer);

        const std::string path = dir + def.name + ".postfx.json";
        app->getFileSystem()->Save(path.c_str(), sb.GetString(), (unsigned)sb.GetSize());
    };

    PostProcessEffectDef chromaticAberration;
    chromaticAberration.name = "ChromaticAberration";
    chromaticAberration.shaderFile = "ChromaticAberrationPS.cso";
    chromaticAberration.domain = PostProcessEffectDef::Domain::PreTonemap;
    chromaticAberration.order = 100;
    chromaticAberration.enabled = false;
    chromaticAberration.params[0] = 0.015f; chromaticAberration.params[1] = 0.008f; chromaticAberration.params[2] = -0.008f;
    chromaticAberration.params[4] = 0.5f; chromaticAberration.params[5] = 0.5f;
    write(chromaticAberration);

    PostProcessEffectDef fxaa;
    fxaa.name = "FXAA";
    fxaa.shaderFile = "FXAAPS.cso";
    fxaa.domain = PostProcessEffectDef::Domain::PostGamma;
    fxaa.order = 100;
    fxaa.enabled = true;
    fxaa.params[0] = 0.0312f; fxaa.params[1] = 0.125f; fxaa.params[2] = 0.75f;
    write(fxaa);
}

void PostProcessChain::reload(ID3D12Device* device){
    m_effects.clear();

    auto* fs = app->getFileSystem();
    const std::string dir = fs->GetAssetsPath() + kPostProcessSubdir;
    fs->CreateDir(dir.c_str());

    std::vector<std::string> files = fs->GetFilesInDirectory(dir.c_str(), ".json");
    if (files.empty()){
        seedDefaultManifests(dir);
        files = fs->GetFilesInDirectory(dir.c_str(), ".json");
    }

    for (const std::string& path : files){
        char* buf = nullptr;
        unsigned size = fs->Load(path.c_str(), &buf);
        if (!buf || size == 0) continue;

        Document doc;
        doc.Parse(buf, size);
        delete[] buf;

        if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("name") || !doc.HasMember("shader")){
            LOG("PostProcessChain: invalid manifest '%s'", path.c_str());
            continue;
        }

        PostProcessEffectDef def;
        def.name = doc["name"].GetString();
        def.shaderFile = doc["shader"].GetString();
        def.domain = doc.HasMember("domain") ? domainFromString(doc["domain"].GetString()) : PostProcessEffectDef::Domain::PreTonemap;
        def.order = doc.HasMember("order") ? doc["order"].GetInt() : 0;
        def.enabled = doc.HasMember("enabled") ? doc["enabled"].GetBool() : true;
        if (doc.HasMember("params") && doc["params"].IsArray()){
            const Value& p = doc["params"];
            for (SizeType i = 0; i < p.Size() && i < 8; ++i)
                def.params[i] = p[i].GetFloat();
        }

        ComPtr<ID3D12PipelineState> pso = buildPSO(device, def);
        if (!pso) continue;

        PostProcessEffect effect;
        effect.def = def;
        effect.pso = pso;
        m_effects.push_back(std::move(effect));
    }

    std::sort(m_effects.begin(), m_effects.end(), [](const PostProcessEffect& a, const PostProcessEffect& b){
        if (a.def.domain != b.def.domain) return a.def.domain < b.def.domain;
        return a.def.order < b.def.order;
    });
}

int PostProcessChain::countEnabled(PostProcessEffectDef::Domain domain) const{
    int count = 0;
    for (const auto& e : m_effects)
        if (e.def.domain == domain && e.def.enabled) ++count;
    return count;
}

void PostProcessChain::drawEffect(ID3D12GraphicsCommandList* cmd, const PostProcessEffect& effect,
                                   RenderTexture* input, RenderTexture* output){
    BEGIN_EVENT(cmd, L"PostProcess Plugin");

    output->beginRender(cmd, false);

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(effect.pso.Get());
    cmd->SetGraphicsRoot32BitConstants(0, 8, effect.def.params, 0);
    cmd->SetGraphicsRootDescriptorTable(1, input->getSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(2, m_fallbackAuxSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(3, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    output->endRender(cmd);

    END_EVENT(cmd);
}

RenderTexture* PostProcessChain::run(ID3D12GraphicsCommandList* cmd, PostProcessEffectDef::Domain domain,
                                     RenderTexture* a, RenderTexture* b){
    RenderTexture* cur = a;
    RenderTexture* other = b;

    for (const auto& effect : m_effects){
        if (effect.def.domain != domain || !effect.def.enabled) continue;
        drawEffect(cmd, effect, cur, other);
        std::swap(cur, other);
    }

    return cur;
}
