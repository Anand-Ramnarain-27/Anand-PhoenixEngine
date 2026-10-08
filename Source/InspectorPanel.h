#pragma once
// The Inspector panel: the selected object's name, tag, transform and components.

#include "EditorPanel.h"
#include <functional>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

class ComponentCamera;
class ComponentMesh;
class ComponentAnimation;
class Material;
class GameObject;
struct ID3D12Resource;
struct PrefabEditSession;

/// Draws each component's onEditor() under a collapsing header and records edits as prefab overrides.
class InspectorPanel : public EditorPanel {
public:
    explicit InspectorPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Inspector"; }

protected:
    void drawContent() override;

private:
    void drawPrefabModeBanner();
    void drawObjectHeader(GameObject* go, bool isEditRoot);
    void drawPrefabEditBar(PrefabEditSession& session, bool isEditRoot);
    void drawPrefabInstanceTag(GameObject* go);
    void drawComponents(GameObject* go);
    void drawSaveAsPrefab(GameObject* go);
    void drawTransform();
    void drawAddComponentMenu();
};
