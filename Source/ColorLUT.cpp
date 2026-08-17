#include "Globals.h"
#include "ColorLUT.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleFileSystem.h"
#include "ModuleShaderDescriptors.h"
#include <d3dx12.h>
#include <sstream>

bool ColorLUT::loadCube(ID3D12Device* device, const std::string& path){
    auto* fs = app->getFileSystem();
    if (!fs->Exists(path.c_str())){
        LOG("ColorLUT: file not found '%s'", path.c_str());
        return false;
    }

    char* buf = nullptr;
    unsigned size = fs->Load(path.c_str(), &buf);
    if (!buf || size == 0) return false;
    std::string content(buf, size);
    delete[] buf;

    std::istringstream stream(content);
    std::string line;
    int lutSize = 0;
    std::vector<float> data;

    while (std::getline(stream, line)){
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("TITLE", 0) == 0 || line.rfind("DOMAIN_MIN", 0) == 0 || line.rfind("DOMAIN_MAX", 0) == 0)
            continue;
        if (line.rfind("LUT_3D_SIZE", 0) == 0){
            sscanf_s(line.c_str(), "LUT_3D_SIZE %d", &lutSize);
            if (lutSize > 0) data.reserve((size_t)lutSize * lutSize * lutSize * 3);
            continue;
        }
        float r, g, b;
        if (sscanf_s(line.c_str(), "%f %f %f", &r, &g, &b) == 3){
            data.push_back(r); data.push_back(g); data.push_back(b);
        }
    }

    if (lutSize <= 0 || data.size() != (size_t)lutSize * lutSize * lutSize * 3){
        LOG("ColorLUT: invalid or incomplete .cube file '%s'", path.c_str());
        return false;
    }

    return createTexture3D(device, lutSize, data);
}

bool ColorLUT::createTexture3D(ID3D12Device* device, int size, const std::vector<float>& data){
    m_size = size;

    D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R32G32B32_FLOAT, size, size, (UINT16)size, 1);
    auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture)))){
        LOG("ColorLUT: failed to create 3D texture resource");
        return false;
    }
    m_texture->SetName(L"ColorLUT");

    UINT64 uploadSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ComPtr<ID3D12Resource> uploadBuf;
    if (FAILED(device->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuf)))){
        LOG("ColorLUT: failed to create upload buffer");
        return false;
    }

    D3D12_SUBRESOURCE_DATA sub = {};
    sub.pData = data.data();
    sub.RowPitch = (LONG_PTR)size * sizeof(float) * 3;
    sub.SlicePitch = sub.RowPitch * size;

    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)))) return false;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)))) return false;

    UpdateSubresources(cmd.Get(), m_texture.Get(), uploadBuf.Get(), 0, 0, 1, &sub);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
    cmd->Close();

    ID3D12CommandList* lists[] = { cmd.Get() };
    app->getD3D12()->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
    app->getD3D12()->flush();

    m_srv = app->getShaderDescriptors()->allocTable("ColorLUT");
    if (!m_srv.isValid()) return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture3D.MipLevels = 1;
    m_srv.createSRV(m_texture.Get(), 0, &srvDesc);

    return true;
}
