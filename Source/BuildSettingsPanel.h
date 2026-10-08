#pragma once
// File > Build Settings: the scenes and output folder of a Player build, and the build button.

#include "EditorPanel.h"
#include "BuildSettings.h"
#include "BuildPipeline.h"
#include <memory>
#include <string>

class FileDialog;

/// Edits BuildSettings (saved to Library/BuildSettings.json) and runs BuildPipeline, showing its progress.
class BuildSettingsPanel : public EditorPanel {
public:
    explicit BuildSettingsPanel(ModuleEditor* editor);
    const char* getName() const override { return "Build Settings"; }

protected:
    void drawContent() override;

private:
    void drawSceneList();
    void drawOutputSection();
    void save();

    BuildSettings m_settings;
    std::string m_settingsPath;
    std::unique_ptr<FileDialog> m_outputDirDialog;
    std::string m_pickedScenePath;
    BuildPipeline::Status m_lastSeenStatus = BuildPipeline::Status::Idle;
};
