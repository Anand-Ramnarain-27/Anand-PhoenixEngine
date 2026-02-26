#pragma once

#include <wrl.h>
#include <d3d12.h>
#include "ShaderTableDesc.h"

using Microsoft::WRL::ComPtr;

class EnvironmentMap
{
public:
    static constexpr uint32_t NUM_ROUGHNESS_LEVELS = 5;  
     
    ComPtr<ID3D12Resource> cubemap;
    ShaderTableDesc        srvTable;          
     
    ComPtr<ID3D12Resource> irradianceCubemap;   
    ShaderTableDesc        irradianceSRVTable;  

    ComPtr<ID3D12Resource> prefilteredCubemap;  
    ShaderTableDesc        prefilteredSRVTable;   

    ComPtr<ID3D12Resource> brdfLUT;         
    ShaderTableDesc        brdfLUTSRVTable;  
     
    bool isValid() const
    {
        return cubemap != nullptr && srvTable.isValid();
    }

    bool hasIBL() const
    {
        return irradianceCubemap != nullptr && irradianceSRVTable.isValid()
            && prefilteredCubemap != nullptr && prefilteredSRVTable.isValid()
            && brdfLUT != nullptr && brdfLUTSRVTable.isValid();
    }
     
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle()      const { return srvTable.getGPUHandle(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getIrradianceGPU()  const { return irradianceSRVTable.getGPUHandle(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getPrefilteredGPU() const { return prefilteredSRVTable.getGPUHandle(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getBRDFLUTGPU()     const { return brdfLUTSRVTable.getGPUHandle(); }
};