#include "Globals.h"
#include "CommandContext.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"

ID3D12Device* CommandContext::getDevice() {
	return m_d3d12 ? m_d3d12->getDevice() : nullptr;
}

ID3D12CommandQueue* CommandContext::getDrawQueue() {
	return m_d3d12 ? m_d3d12->getDrawCommandQueue() : nullptr;
}