#pragma once
#include "EditorPanel.h"
#include "FileDialog.h"
#include <string>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

class AssetBrowserPanel : public EditorPanel
{
public:
    explicit AssetBrowserPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    void draw() override;
    const char* getName() const override { return "Asset Browser"; }

private:
    void drawToolbar();
    void drawImportSidebar(float panelW, float panelH);
    void drawAssetList(float contentW, float panelH);
    void drawActionBar();

    bool drawAssetRow(const std::string& path, const std::string& icon,
        const std::string& type, const std::string& extra = "");

    void handleAddToScene();

    int         m_filter = 0;
    int         m_selectedIdx = -1;
    std::string m_selectedPath;
    std::string m_selectedType;

    ComPtr<ID3D12Resource>      m_pendingTexture;
    D3D12_GPU_DESCRIPTOR_HANDLE m_pendingTextureSRV = {};
    std::string                 m_pendingTexturePath;

    FileDialog m_modelBrowseDialog;
    FileDialog m_texBrowseDialog;
};