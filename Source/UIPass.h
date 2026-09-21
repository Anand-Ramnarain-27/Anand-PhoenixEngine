#pragma once
#include "Globals.h"
#include "ShaderTableDesc.h"
#include "UIDrawItem.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

// Renders UI draw lists with DirectXTK12's SpriteBatch/SpriteFont into an LDR (post-tonemap) target.
// Images use a straight-alpha batch and text a premultiplied-alpha batch (the baked font format).
class UIPass {
public:
    static constexpr DXGI_FORMAT kTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    bool init(ID3D12Device* device, ID3D12CommandQueue* queue);
    void shutdown();

    // Bake-once .spritefont loaded from the exe directory (or an absolute path). Returns false on failure.
    bool loadFont(const std::string& name, const std::wstring& path);
    bool hasFont(const std::string& name) const { return m_fonts.count(name) != 0; }

    // Size of `text` in unscaled font pixels; zero if the font is unknown.
    Vector2 measureText(const std::string& font, const std::string& text) const;
    float getLineSpacing(const std::string& font) const;

    // The target must already be bound as a render target and the shader descriptor heap set.
    void render(ID3D12GraphicsCommandList* cmd, const std::vector<UIDrawItem>& items, uint32_t width, uint32_t height);

private:
    struct FontEntry {
        std::unique_ptr<DirectX::SpriteFont> font;
        ShaderTableDesc srv;
    };

    struct TextureEntry {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc srv;
        DirectX::XMUINT2 size = { 1, 1 };
    };

    const TextureEntry& getTexture(const std::string& path);
    bool createWhiteTexture();

    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_queue = nullptr;

    std::unique_ptr<DirectX::SpriteBatch> m_imageBatch;
    std::unique_ptr<DirectX::SpriteBatch> m_textBatch;

    std::unordered_map<std::string, FontEntry> m_fonts;
    std::unordered_map<std::string, TextureEntry> m_textures;
    TextureEntry m_white;
};
