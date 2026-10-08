#pragma once
// The Scene Settings panel: skybox, ambient lighting, physics and broad-phase options.

#include "EditorPanel.h"
#include <vector>
#include <string>

/// Edits the active scene's EditorSceneSettings, which are saved with the scene.
class SceneSettingsPanel : public EditorPanel {
public:
    explicit SceneSettingsPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Scene Settings"; }

protected:
    void drawContent() override;

private:
    void drawEnvironmentSection();
    void drawLightingSection();
    void drawPhysicsSection();
    void drawBroadphaseSection();

    std::vector<std::string> m_skyboxFiles;
    int m_selectedSkybox = -1;
    bool m_scanned = false;
};
