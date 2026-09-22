#include "Globals.h"
#include "UIPass.h"
#include "Application.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ReadData.h"
#include <CommonStates.h>
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace DirectX;

namespace {
    // The UI target holds display-referred (already gamma-encoded) values, so textures are sampled
    // raw: view sRGB formats as their UNORM twin instead of letting the sampler linearize them.
    DXGI_FORMAT toLinearView(DXGI_FORMAT f){
        switch (f){
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:      return DXGI_FORMAT_BC1_UNORM;
        case DXGI_FORMAT_BC2_UNORM_SRGB:      return DXGI_FORMAT_BC2_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:      return DXGI_FORMAT_BC3_UNORM;
        case DXGI_FORMAT_BC7_UNORM_SRGB:      return DXGI_FORMAT_BC7_UNORM;
        default: return f;
        }
    }

    XMVECTOR premultiplied(const Vector4& c){
        return XMVectorSet(c.x * c.w, c.y * c.w, c.z * c.w, c.w);
    }
}

bool UIPass::init(ID3D12Device* device, ID3D12CommandQueue* queue){
    m_device = device;
    m_queue = queue;

    try {
        ResourceUploadBatch upload(device);
        upload.Begin();

        RenderTargetState rtState(kTargetFormat, DXGI_FORMAT_UNKNOWN);
        SpriteBatchPipelineStateDescription imagePso(rtState, &CommonStates::NonPremultiplied, &CommonStates::DepthNone, &CommonStates::CullNone);
        SpriteBatchPipelineStateDescription textPso(rtState, &CommonStates::AlphaBlend, &CommonStates::DepthNone, &CommonStates::CullNone);
        m_imageBatch = std::make_unique<SpriteBatch>(device, upload, imagePso);
        m_textBatch = std::make_unique<SpriteBatch>(device, upload, textPso);

        upload.End(queue).wait();
    }
    catch (const std::exception& e){
        LOG("UIPass: SpriteBatch creation failed: %s", e.what());
        return false;
    }

    if (!createWhiteTexture()) return false;

    LOG("UIPass: init OK");
    return true;
}

void UIPass::shutdown(){
    m_fonts.clear();
    m_textures.clear();
    m_white = {};
    m_imageBatch.reset();
    m_textBatch.reset();
}

bool UIPass::createWhiteTexture(){
    const uint32_t white = 0xFFFFFFFFu;
    m_white.resource = app->getGPUResources()->createRawTexture2D(&white, sizeof(white), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_white.resource){
        LOG("UIPass: white texture creation failed");
        return false;
    }
    m_white.resource->SetName(L"UI_WhiteTexture");
    m_white.srv = app->getShaderDescriptors()->allocTable("UI_WhiteSRV");
    if (!m_white.srv.isValid()) return false;
    m_white.srv.createTexture2DSRV(m_white.resource.Get(), 0);
    m_white.size = { 1, 1 };
    return true;
}

bool UIPass::loadFont(const std::string& name, const std::wstring& path){
    if (!m_device) return false;

    try {
        std::vector<uint8_t> blob = DX::ReadData(path.c_str());

        FontEntry entry;
        entry.srv = app->getShaderDescriptors()->allocTable(("UI_Font_" + name).c_str());
        if (!entry.srv.isValid()){
            LOG("UIPass: no descriptor table for font '%s'", name.c_str());
            return false;
        }

        ResourceUploadBatch upload(m_device);
        upload.Begin();
        entry.font = std::make_unique<SpriteFont>(m_device, upload, blob.data(), blob.size(),
                                                  entry.srv.getCPUHandle(0), entry.srv.getGPUHandle(0));
        upload.End(m_queue).wait();

        m_fonts[name] = std::move(entry);
    }
    catch (const std::exception& e){
        LOG("UIPass: failed to load font '%s': %s", name.c_str(), e.what());
        return false;
    }

    LOG("UIPass: loaded font '%s'", name.c_str());
    return true;
}

Vector2 UIPass::measureText(const std::string& font, const std::string& text, bool ignoreWhitespace) const{
    auto it = m_fonts.find(font);
    if (it == m_fonts.end() || text.empty()) return Vector2::Zero;

    XMFLOAT2 size;
    XMStoreFloat2(&size, it->second.font->MeasureString(text.c_str(), ignoreWhitespace));
    return Vector2(size.x, size.y);
}

float UIPass::getLineSpacing(const std::string& font) const{
    auto it = m_fonts.find(font);
    return it == m_fonts.end() ? 0.f : it->second.font->GetLineSpacing();
}

const UIPass::TextureEntry& UIPass::getTexture(const std::string& path){
    if (path.empty()) return m_white;

    auto it = m_textures.find(path);
    if (it != m_textures.end()) return it->second.resource ? it->second : m_white;

    auto* gpu = app->getGPUResources();
    ComPtr<ID3D12Resource> tex = gpu->createTextureFromFile(path, false);
    if (!tex){
        // Imported textures live in Library/Textures as .dds under the source file's stem.
        namespace fs = std::filesystem;
        const std::string library = "Library/Textures/" + fs::path(path).stem().string() + ".dds";
        tex = gpu->createTextureFromFile(library, false);
    }

    TextureEntry entry;
    if (tex){
        entry.srv = app->getShaderDescriptors()->allocTable(("UI_SRV_" + path).c_str());
        if (entry.srv.isValid()){
            const D3D12_RESOURCE_DESC desc = tex->GetDesc();
            entry.srv.createTexture2DSRV(tex.Get(), 0, toLinearView(desc.Format));
            entry.size = { (uint32_t)desc.Width, desc.Height };
            entry.resource = std::move(tex);
        }
    }
    if (!entry.resource) LOG("UIPass: failed to load texture '%s', drawing white", path.c_str());

    auto inserted = m_textures.emplace(path, std::move(entry));
    return inserted.first->second.resource ? inserted.first->second : m_white;
}

void UIPass::render(ID3D12GraphicsCommandList* cmd, const std::vector<UIDrawItem>& items, uint32_t width, uint32_t height){
    if (!m_imageBatch || !m_textBatch || items.empty()) return;

    const D3D12_VIEWPORT viewport = { 0.f, 0.f, float(width), float(height), 0.f, 1.f };
    m_imageBatch->SetViewport(viewport);
    m_textBatch->SetViewport(viewport);

    // Consecutive items of one kind and one clip share a Begin/End; a change flushes the previous batch.
    const D3D12_RECT fullRect = { 0, 0, (LONG)width, (LONG)height };
    D3D12_RECT scissor = fullRect;
    SpriteBatch* open = nullptr;
    auto use = [&](SpriteBatch* batch){
        if (open == batch) return;
        if (open) open->End();
        open = batch;
        open->Begin(cmd);
    };

    for (const UIDrawItem& item : items){
        D3D12_RECT wanted = fullRect;
        if (item.clip){
            wanted.left = std::clamp((LONG)std::floor(item.clipRect.x), 0L, (LONG)width);
            wanted.top = std::clamp((LONG)std::floor(item.clipRect.y), 0L, (LONG)height);
            wanted.right = std::clamp((LONG)std::ceil(item.clipRect.x + item.clipRect.z), wanted.left, (LONG)width);
            wanted.bottom = std::clamp((LONG)std::ceil(item.clipRect.y + item.clipRect.w), wanted.top, (LONG)height);
        }
        if (wanted.left != scissor.left || wanted.top != scissor.top || wanted.right != scissor.right || wanted.bottom != scissor.bottom){
            if (open){ open->End(); open = nullptr; }
            scissor = wanted;
            cmd->RSSetScissorRects(1, &scissor);
        }

        if (item.kind == UIDrawItem::Kind::Image){
            use(m_imageBatch.get());

            const TextureEntry& tex = getTexture(item.texture);
            RECT src = { 0, 0, (LONG)tex.size.x, (LONG)tex.size.y };
            if (item.useSourceRect){
                src = { (LONG)item.sourceRect.x, (LONG)item.sourceRect.y,
                        (LONG)(item.sourceRect.x + item.sourceRect.z), (LONG)(item.sourceRect.y + item.sourceRect.w) };
            }
            else if (item.useSourceUV){
                const float tw = float(tex.size.x), th = float(tex.size.y);
                src = { (LONG)std::lround(item.sourceUV.x * tw), (LONG)std::lround(item.sourceUV.y * th),
                        (LONG)std::lround((item.sourceUV.x + item.sourceUV.z) * tw),
                        (LONG)std::lround((item.sourceUV.y + item.sourceUV.w) * th) };
            }
            const float srcW = float(src.right - src.left);
            const float srcH = float(src.bottom - src.top);
            if (srcW <= 0.f || srcH <= 0.f) continue;

            m_imageBatch->Draw(tex.srv.getGPUHandle(0), tex.size, item.position, &src,
                               XMVectorSet(item.color.x, item.color.y, item.color.z, item.color.w),
                               item.rotation, XMFLOAT2(item.pivot.x * srcW, item.pivot.y * srcH),
                               XMFLOAT2(item.size.x / srcW, item.size.y / srcH));
        }
        else {
            auto font = m_fonts.find(item.font);
            if (font == m_fonts.end()) continue;
            use(m_textBatch.get());

            font->second.font->DrawString(m_textBatch.get(), item.text.c_str(), item.position,
                                          premultiplied(item.color), item.rotation, item.origin, item.scale);
        }
    }

    if (open) open->End();

    if (scissor.left != fullRect.left || scissor.top != fullRect.top || scissor.right != fullRect.right || scissor.bottom != fullRect.bottom)
        cmd->RSSetScissorRects(1, &fullRect);
}
