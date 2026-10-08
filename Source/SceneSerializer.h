#pragma once
// Scene files (Library/Scenes/*.json): every GameObject with its transform and components, plus the scene settings.

#include <string>
#include "EditorSceneSettings.h"

class SceneGraph;

/// Reads and writes scene JSON. Components serialize themselves (Component::onSave / onLoad); this class owns the
/// file layout, the hierarchy (by UID / ParentUID) and the "Settings" block. Loading replaces the scene's contents.
class SceneSerializer {
public:
    /// `settings` null: no "Settings" block is written.
    static bool SaveScene(const SceneGraph* scene, const std::string& filePath, const EditorSceneSettings* settings = nullptr);
    /// `settings` null: the file's settings are ignored; otherwise every saved setting not in the file resets to its default.
    static bool LoadScene(const std::string& filePath, SceneGraph* scene, EditorSceneSettings* settings = nullptr);
    /// Library/Scenes/temp_scene.json: Play saves the edited scene there and Stop restores it.
    static bool SaveTempScene(const SceneGraph* scene);
    static bool LoadTempScene(SceneGraph* scene);
};
