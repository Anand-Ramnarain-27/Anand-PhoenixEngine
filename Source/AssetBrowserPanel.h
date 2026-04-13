#pragma once
#include "EditorPanel.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

struct ID3D12Resource;
struct D3D12_GPU_DESCRIPTOR_HANDLE;
namespace Microsoft::WRL { template<typename T> class ComPtr; }

static constexpr const char* kDragAsset = "ASSET_PATH";

class AssetBrowserPanel : public EditorPanel {
public:
    explicit AssetBrowserPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Asset Browser"; }

protected:
    void drawContent() override;

private:
    void drawFolderTree();
    void drawBreadcrumb();
    void drawThumbnailGrid();
    void drawStatusBar();
    void drawModals();
    void drawFolderNode(const std::string& absPath, bool forceOpen = false);
    void drawItemContextMenu(int idx);
    void drawEmptyAreaContextMenu();
    void drawPrefabInstanceBar();

    void navigateTo(const std::string& absPath);
    void refreshCurrentDir();

    bool isAssetFile(const std::string& ext) const;
    void spawnAsset(const std::string& path);
    void drawColoredBox(ImVec2 localPos, ImVec4 col, const char* label);
    ImVec4 typeColor(const std::string& ext) const;
    const char* typeIcon(const std::string& ext) const;

    void prefabSaveSelected(const char* name);
    void prefabInstantiate(const std::string& name);
    void prefabApply();
    void prefabRevert();
    void prefabDelete(const std::string& name);
    void prefabCreateVariant(const std::string& src, const std::string& dst);
    void prefabRename(const std::string& oldName, const std::string& newName);

    void assignAnimToSelection(const std::string& animPath);
    void assignSmToSelection(const std::string& smPath);

    struct RootFolder { std::string path; std::string label; };
    std::vector<RootFolder> m_roots;

    std::string m_currentPath;
    std::string m_selectedPath;

    struct Entry { std::string path, name, ext; bool isDir = false; };
    std::vector<Entry> m_entries;
    bool m_dirty = true;

    char m_searchBuf[128] = {};

    struct ThumbEntry {
        Microsoft::WRL::ComPtr<ID3D12Resource> tex;
        D3D12_GPU_DESCRIPTOR_HANDLE srv = {};
        bool attempted = false;
    };
    std::unordered_map<std::string, ThumbEntry> m_thumbCache;

    bool m_showVariantModal = false;
    bool m_showSavePrefabModal = false;
    bool m_renamingPrefab = false;
    char m_variantSrcBuf[128] = {};
    char m_variantDstBuf[128] = {};
    char m_savePrefabNameBuf[128] = {};
    char m_renameSrcBuf[128] = {};
    char m_renameDstBuf[128] = {};

    static constexpr float kThumbSize = 72.0f;
    static constexpr float kTreeW = 180.0f;
    static constexpr float kStatusH = 30.0f;
};