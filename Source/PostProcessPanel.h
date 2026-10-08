#pragma once
// The Post Process panel: tonemapping, bloom, fog, colour grading, occlusion / x-ray and plugin effects.

#include "EditorPanel.h"
#include <vector>
#include <string>

/// Edits the active scene's post-process settings (EditorSceneSettings), which are saved with the scene.
class PostProcessPanel : public EditorPanel {
public:
    explicit PostProcessPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Post Process"; }

protected:
    void drawContent() override;

private:
    void drawTonemapSection();
    void drawBloomSection();
    void drawFogSection();
    void drawLutSection();
    void drawOcclusionXRaySection();
    void drawPluginEffectsSection();

    std::vector<std::string> m_lutFiles;
    int m_selectedLut = -1;
    bool m_scannedLuts = false;
};
