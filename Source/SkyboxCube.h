#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

class SkyboxCube
{
public:
    SkyboxCube();
    ~SkyboxCube();

    void draw(ID3D12GraphicsCommandList* cmdList);
    const D3D12_INPUT_LAYOUT_DESC& getInputLayout() const;

private:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbView;
    D3D12_INPUT_LAYOUT_DESC m_inputLayout;
    D3D12_INPUT_ELEMENT_DESC m_layout;
};