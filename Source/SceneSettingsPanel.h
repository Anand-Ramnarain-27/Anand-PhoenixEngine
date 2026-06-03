#pragma once
#include "EditorPanel.h"
#include <vector>
#include <string>

class SceneSettingsPanel : public EditorPanel {
public:
    explicit SceneSettingsPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Scene Settings"; }

protected:
    void drawContent() override;

private:
    std::vector<std::string> m_skyboxFiles;
    int  m_selectedSkybox = -1;
    bool m_scanned        = false;
};
