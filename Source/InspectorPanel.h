#pragma once
#include "EditorPanel.h"
#include <functional>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

class ComponentCamera;
class ComponentMesh;
class ComponentAnimation;
class Material;
struct ID3D12Resource;

class InspectorPanel : public EditorPanel {
public:
    explicit InspectorPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Inspector"; }

protected:
    void drawContent() override;

private:
    void drawTransform();
    void drawAddComponentMenu();
};
