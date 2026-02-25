#include "Globals.h"
#include "SkyboxCube.h"
#include <DirectXMath.h>

using namespace DirectX;

static float cubeVertices[] =
{
    -1,-1,-1,  -1,1,-1,  1,1,-1,
    -1,-1,-1,   1,1,-1,  1,-1,-1,

    -1,-1,1,   1,1,1,  -1,1,1,
    -1,-1,1,   1,-1,1,  1,1,1,

    -1,1,-1,  -1,1,1,   1,1,1,
    -1,1,-1,   1,1,1,   1,1,-1,

    -1,-1,-1,  1,-1,1,  -1,-1,1,
    -1,-1,-1,  1,-1,-1, 1,-1,1,

    -1,-1,-1, -1,-1,1, -1,1,1,
    -1,-1,-1, -1,1,1,  -1,1,-1,

    1,-1,-1,  1,1,1,  1,-1,1,
    1,-1,-1,  1,1,-1, 1,1,1
};

SkyboxCube::SkyboxCube()
{
    // Assume resource creation handled by your resource system
}

SkyboxCube::~SkyboxCube() {}

void SkyboxCube::draw(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(36, 1, 0, 0);
}

const D3D12_INPUT_LAYOUT_DESC& SkyboxCube::getInputLayout() const
{
    return m_inputLayout;
}