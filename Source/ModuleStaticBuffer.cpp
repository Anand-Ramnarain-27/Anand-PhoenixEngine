#include "Globals.h"
#include "ModuleStaticBuffer.h"

bool ModuleStaticBuffer::init(ID3D12Device* device, size_t poolSizeBytes){
    if (m_initialized){
        LOG("ModuleStaticBuffer: already initialized — call shutdown() first.");
        return false;
    }

    m_poolSize = poolSizeBytes;

    {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(poolSizeBytes);

        HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_defaultHeapBuffer));

        if (FAILED(hr)){
            LOG("ModuleStaticBuffer: failed to create DEFAULT heap buffer (%zu MB)",
                poolSizeBytes / (1024 * 1024));
            return false;
        }
        m_defaultHeapBuffer->SetName(L"StaticBuffer_DEFAULT");
    }

    {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(poolSizeBytes);

        HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_uploadHeapBuffer));

        if (FAILED(hr)){
            LOG("ModuleStaticBuffer: failed to create UPLOAD heap buffer");
            m_defaultHeapBuffer.Reset();
            return false;
        }
        m_uploadHeapBuffer->SetName(L"StaticBuffer_UPLOAD");

        m_uploadHeapBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_uploadPtr));
    }

    m_defaultOffset = 0;
    m_cbOffset = 0;
    m_initialized = true;

    LOG("ModuleStaticBuffer: initialized  pool = %zu MB",
        poolSizeBytes / (1024 * 1024));
    return true;
}

size_t ModuleStaticBuffer::suballocate(size_t sizeBytes){
    size_t aligned = alignUp(sizeBytes);
    if (m_defaultOffset + aligned > m_poolSize){
        LOG("ModuleStaticBuffer: DEFAULT heap FULL  (used %zu / %zu bytes)",
            m_defaultOffset, m_poolSize);
        return SIZE_MAX;
    }
    size_t offset = m_defaultOffset;
    m_defaultOffset += aligned;
    return offset;
}

size_t ModuleStaticBuffer::suballocateCB(size_t sizeBytes){
    size_t aligned = alignUp(sizeBytes);
    if (m_cbOffset + aligned > m_poolSize)
    {
        LOG("ModuleStaticBuffer: UPLOAD heap FULL for CB  (used %zu / %zu bytes)",
            m_cbOffset, m_poolSize);
        return SIZE_MAX;
    }
    size_t offset = m_cbOffset;
    m_cbOffset += aligned;
    return offset;
}

D3D12_VERTEX_BUFFER_VIEW ModuleStaticBuffer::allocVertexBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, size_t sizeBytes, UINT strideBytes, const std::string& debugName){
    D3D12_VERTEX_BUFFER_VIEW vbv = {};

    if (!m_initialized || !srcData || sizeBytes == 0) return vbv;

    size_t offset = suballocate(sizeBytes);
    if (offset == SIZE_MAX) return vbv;

    memcpy(m_uploadPtr + offset, srcData, sizeBytes);

    auto barrierToCopy = CD3DX12_RESOURCE_BARRIER::Transition( m_defaultHeapBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->ResourceBarrier(1, &barrierToCopy);

    cmd->CopyBufferRegion(m_defaultHeapBuffer.Get(), offset, m_uploadHeapBuffer.Get(), offset, sizeBytes);

    auto barrierToVB = CD3DX12_RESOURCE_BARRIER::Transition(m_defaultHeapBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmd->ResourceBarrier(1, &barrierToVB);

    vbv.BufferLocation = m_defaultHeapBuffer->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes = static_cast<UINT>(sizeBytes);
    vbv.StrideInBytes = strideBytes;

    if (!debugName.empty())
        LOG("ModuleStaticBuffer: VB '%s'  offset=%zu  size=%zu",
            debugName.c_str(), offset, sizeBytes);

    return vbv;
}

D3D12_INDEX_BUFFER_VIEW ModuleStaticBuffer::allocIndexBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, size_t sizeBytes, DXGI_FORMAT  format, const std::string& debugName){
    D3D12_INDEX_BUFFER_VIEW ibv = {};

    if (!m_initialized || !srcData || sizeBytes == 0) return ibv;

    size_t offset = suballocate(sizeBytes);
    if (offset == SIZE_MAX) return ibv;

    memcpy(m_uploadPtr + offset, srcData, sizeBytes);

    auto barrierToCopy = CD3DX12_RESOURCE_BARRIER::Transition(m_defaultHeapBuffer.Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->ResourceBarrier(1, &barrierToCopy);

    cmd->CopyBufferRegion(m_defaultHeapBuffer.Get(), offset, m_uploadHeapBuffer.Get(), offset, sizeBytes);

    auto barrierToIB = CD3DX12_RESOURCE_BARRIER::Transition(m_defaultHeapBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    cmd->ResourceBarrier(1, &barrierToIB);

    ibv.BufferLocation = m_defaultHeapBuffer->GetGPUVirtualAddress() + offset;
    ibv.SizeInBytes = static_cast<UINT>(sizeBytes);
    ibv.Format = format;

    if (!debugName.empty())
        LOG("ModuleStaticBuffer: IB '%s'  offset=%zu  size=%zu",
            debugName.c_str(), offset, sizeBytes);

    return ibv;
}

D3D12_GPU_VIRTUAL_ADDRESS ModuleStaticBuffer::allocConstantBuffer(size_t sizeBytes, void** outCpuPtr, const std::string& debugName){
    if (!m_initialized || sizeBytes == 0) return 0;

    size_t offset = suballocateCB(sizeBytes);
    if (offset == SIZE_MAX) return 0;

    if (outCpuPtr) *outCpuPtr = m_uploadPtr + offset;

    if (!debugName.empty())
        LOG("ModuleStaticBuffer: CB '%s'  offset=%zu  size=%zu",
            debugName.c_str(), offset, sizeBytes);

    return m_uploadHeapBuffer->GetGPUVirtualAddress() + offset;
}

void ModuleStaticBuffer::reset(){
    m_defaultOffset = 0;
    m_cbOffset = 0;
    LOG("ModuleStaticBuffer: reset  (pool recycled for next level)");
}

void ModuleStaticBuffer::shutdown(){
    if (m_uploadHeapBuffer) m_uploadHeapBuffer->Unmap(0, nullptr);
    m_uploadPtr = nullptr;

    m_defaultHeapBuffer.Reset();
    m_uploadHeapBuffer.Reset();

    m_defaultOffset = 0;
    m_cbOffset = 0;
    m_poolSize = 0;
    m_initialized = false;

    LOG("ModuleStaticBuffer: shutdown.");
}