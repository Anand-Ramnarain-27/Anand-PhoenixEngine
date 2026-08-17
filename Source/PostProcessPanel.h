#pragma once
#include "EditorPanel.h"
#include <vector>
#include <string>

class PostProcessPanel : public EditorPanel {
public:
    explicit PostProcessPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Post Process"; }

protected:
    void drawContent() override;

private:
    void drawTonemapSection();
    void drawBloomSection();
    void drawLutSection();
    void drawPluginEffectsSection();

    std::vector<std::string> m_lutFiles;
    int m_selectedLut = -1;
    bool m_scannedLuts = false;
};
