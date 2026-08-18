#pragma once
#include "EditorPanel.h"
#include "BuildSettings.h"
#include <memory>
#include <string>

class FileDialog;

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
};
