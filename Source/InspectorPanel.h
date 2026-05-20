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
    explicit InspectorPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Inspector"; }

protected:
    void drawContent() override;

private:
    void drawTransform();
    void drawComponentCamera(ComponentCamera* cam);
    void drawComponentMesh(ComponentMesh* mesh);
    void drawComponentAnimation(ComponentAnimation* anim);
    void drawAddComponentMenu();
    void drawTexturePicker(ComponentMesh* mesh, Material* mat, int submeshIdx,
        const char* label, bool hasTex, const char* tooltip,
        std::function<void(ComPtr<ID3D12Resource>, D3D12_GPU_DESCRIPTOR_HANDLE)> onApply);
};