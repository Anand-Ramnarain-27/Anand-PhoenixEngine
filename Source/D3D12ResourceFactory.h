#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace D3D12ResourceFactory {
	inline bool createCubemapRT(ID3D12Device* device, uint32_t faceSize, uint32_t mipLevels, DXGI_FORMAT fmt, const wchar_t* debugName, ComPtr<ID3D12Resource>& out) {
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(fmt, faceSize, faceSize, 6, static_cast<UINT16>(mipLevels), 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&out));

		if (FAILED(hr)) {
			return false;
		}

		if (debugName)
			out->SetName(debugName);

		return true;
	}

	inline bool create2DRT(ID3D12Device* device, uint32_t size, DXGI_FORMAT fmt, const wchar_t* debugName, ComPtr<ID3D12Resource>& out) {
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(fmt, size, size, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&out));

		if (FAILED(hr))
			return false;

		if (debugName)
			out->SetName(debugName);

		return true;
	}
}