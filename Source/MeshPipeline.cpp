#include "Globals.h"
#include "MeshPipeline.h"
#include "EnvironmentSystem.h"
#include "EnvironmentMap.h"
#include "ModuleSamplerHeap.h"
#include "Mesh.h"
#include "ReadData.h"
#include <d3dx12.h>

bool MeshPipeline::init(ID3D12Device* device, bool useMSAA) {
	return createRootSignature(device) && createPSO(device, useMSAA);
}

bool MeshPipeline::createRootSignature(ID3D12Device* device) {
	CD3DX12_DESCRIPTOR_RANGE dirRange;
	dirRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE pointRange;
	pointRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE spotRange;
	spotRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE iblIrradianceRange;
	iblIrradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

	CD3DX12_DESCRIPTOR_RANGE iblPrefilterRange;
	iblPrefilterRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

	CD3DX12_DESCRIPTOR_RANGE iblBrdfRange;
	iblBrdfRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

	CD3DX12_DESCRIPTOR_RANGE matRange;
	matRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 6);

	CD3DX12_DESCRIPTOR_RANGE samplerRange;
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

	CD3DX12_ROOT_PARAMETER params[11];
	params[SLOT_MVP_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	params[SLOT_PERFRAME_CB].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_PERINSTANCE_CB].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);
	params[SLOT_DIR_LIGHTS].InitAsDescriptorTable(1, &dirRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_POINT_LIGHTS].InitAsDescriptorTable(1, &pointRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_SPOT_LIGHTS].InitAsDescriptorTable(1, &spotRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_IRRADIANCE].InitAsDescriptorTable(1, &iblIrradianceRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_PREFILTER].InitAsDescriptorTable(1, &iblPrefilterRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_BRDF_LUT].InitAsDescriptorTable(1, &iblBrdfRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_MAT_TEXTURES].InitAsDescriptorTable(1, &matRange, D3D12_SHADER_VISIBILITY_PIXEL);
	params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (FAILED(hr)) {
		if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
		LOG("MeshPipeline: D3D12SerializeRootSignature failed 0x%08X", hr);
		return false;
	}

	hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	if (FAILED(hr)) {
		LOG("MeshPipeline: CreateRootSignature failed 0x%08X", hr);
		return false;
	}
	return true;
}

bool MeshPipeline::createPSO(ID3D12Device* device, bool useMSAA) {
	auto vs = DX::ReadData(L"PBRForwardVS.cso");
	auto ps = DX::ReadData(L"PBRForwardPS.cso");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = m_rootSig.Get();
	desc.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
	desc.VS = { vs.data(), vs.size() };
	desc.PS = { ps.data(), ps.size() };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.NumRenderTargets = 1;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc = { useMSAA ? UINT(4) : UINT(1), 0 };
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.FrontCounterClockwise = TRUE;
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
	if (FAILED(hr)) {
		LOG("MeshPipeline: CreateGraphicsPipelineState failed 0x%08X", hr);
		return false;
	}
	return true;
}

void MeshPipeline::bindIBL(ID3D12GraphicsCommandList* cmd, const EnvironmentSystem* env) const {
	if (!env || !env->hasIBL()) return;
	const EnvironmentMap* map = env->getEnvironmentMap();
	cmd->SetGraphicsRootDescriptorTable(SLOT_IRRADIANCE, map->getIrradianceGPU());
	cmd->SetGraphicsRootDescriptorTable(SLOT_PREFILTER, map->getPrefilteredGPU());
	cmd->SetGraphicsRootDescriptorTable(SLOT_BRDF_LUT, map->getBRDFLUTGPU());
}