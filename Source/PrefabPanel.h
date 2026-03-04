#pragma once
#include "EditorPanel.h"
#include <string>

class PrefabPanel : public EditorPanel
{
public:
    explicit PrefabPanel(ModuleEditor* editor) : EditorPanel(editor) {}

    void draw() override;
    const char* getName() const override { return "Prefabs"; }

private:
    void drawToolbar();
    void drawPrefabList(float listH);
    void drawInstanceControls();
    void drawCreateFromSelection();

    void doCreate(const std::string& name);
    void doInstantiate(const std::string& name);
    void doApply();
    void doRevert();
    void doDelete(const std::string& name);
    void doCreateVariant(const std::string& srcName, const std::string& dstName);

    char m_searchBuf[128] = {};

    std::string m_selectedPrefab;

    bool m_renaming = false;
    char m_renameBuf[128] = {};

    bool m_showVariantPopup = false;
    char m_variantNameBuf[128] = {};

    char m_newPrefabNameBuf[128] = {};
};