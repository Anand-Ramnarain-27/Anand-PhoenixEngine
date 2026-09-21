#pragma once

class SceneGraph;
class HotReloadManager;

// Adds a camera (made the active one), a light and a Canvas that shows every UI feature: corner/centre/stretch
// anchors, rotation, Image and Label alignment, and buttons in normal/disabled states. If the script DLL with
// UIDemoScript is loaded, the first button gets it so clicks are counted on screen. Existing objects are kept.
void CreateUITestScene(SceneGraph* scene, HotReloadManager* hotReload);
