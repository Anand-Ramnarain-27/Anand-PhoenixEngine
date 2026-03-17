#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "Globals.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"

using Microsoft::WRL::ComPtr;

class CommandContext {
public:
	CommandContext(ModuleD3D12* d3d12, ModuleShaderDescriptors* shaderDesc, ModuleSamplerHeap* samplerHeap)
		: m_d3d12(d3d12), m_shaderDesc(shaderDesc), m_samplerHeap(samplerHeap) {
		m_valid = init();
	}

	bool isValid() const { return m_valid; }
	ID3D12GraphicsCommandList* cmd() const { return m_cmd.Get(); }

	bool submitAndReset(const char* stage) {
		if (!closeAndExecute(stage))
			return false;

		if (FAILED(m_alloc->Reset())) {
			LOG("CommandContext: alloc Reset() failed after '%s'", stage);
			return false;
		}

		if (FAILED(m_cmd->Reset(m_alloc.Get(), nullptr))) {
			LOG("CommandContext: cmd Reset() failed after '%s'", stage);
			return false;
		}

		bindHeaps();
		return true;
	}

	bool submit(const char* stage) {
		return closeAndExecute(stage);
	}

private:
	bool init() {
		ID3D12Device* device = getDevice();
		if (!device) return false;

		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_alloc))))
			return false;

		if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_alloc.Get(), nullptr, IID_PPV_ARGS(&m_cmd))))
			return false;

		bindHeaps();
		return true;
	}

	bool closeAndExecute(const char* stage) {
		if (FAILED(m_cmd->Close())) {
			LOG("CommandContext: cmd->Close() failed at '%s'", stage);
			return false;
		}

		ID3D12CommandList* lists[] = { m_cmd.Get() };
		getDrawQueue()->ExecuteCommandLists(1, lists);
		getD3D12()->flush();

		if (FAILED(getDevice()->GetDeviceRemovedReason())) {
			LOG("CommandContext: device removed after '%s'!", stage);
			return false;
		}

		return true;
	}

	void bindHeaps() {
		ID3D12DescriptorHeap* heaps[] = { m_shaderDesc->getHeap(), m_samplerHeap->getHeap() };
		m_cmd->SetDescriptorHeaps(2, heaps);
	}

	ID3D12Device* getDevice();
	ID3D12CommandQueue* getDrawQueue();
	ModuleD3D12* getD3D12() { return m_d3d12; }

	ModuleD3D12* m_d3d12 = nullptr;
	ModuleShaderDescriptors* m_shaderDesc = nullptr;
	ModuleSamplerHeap* m_samplerHeap = nullptr;
	ComPtr<ID3D12CommandAllocator> m_alloc;
	ComPtr<ID3D12GraphicsCommandList> m_cmd;
	bool m_valid = false;
};