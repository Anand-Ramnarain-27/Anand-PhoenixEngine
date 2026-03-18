#include "Globals.h"
#include "SkyboxRenderer.h"
#include "Application.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleGPUResources.h"
#include "CubeGeometry.h"
#include "ReadData.h"

using namespace DirectX;

bool SkyboxRenderer::init(ID3D12Device* device, bool useMSAA) {
	return createGeometry(device) && createConstantBuffer(device) && createRootSignature(device) && createPipeline(device, useMSAA);
}

bool SkyboxRenderer::createGeometry(ID3D12Device* device) {
	auto* resources = app->getGPUResources();

	vertexBuffer = resources->createDefaultBuffer(CubeGeometry::kCubeVerts, CubeGeometry::kCubeVertexSize, "SkyboxVB");

	if (!vertexBuffer) return false;

	vertexCount = CubeGeometry::kCubeVertexCount;
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.StrideInBytes = CubeGeometry::kCubeVertexStride;
	vbView.SizeInBytes = CubeGeometry::kCubeVertexSize;
	return true;
}

bool SkyboxRenderer::createConstantBuffer(ID3D12Device* device) {
	auto* resources = app->getGPUResources();

	SkyboxCB zeroCB = {};
	constantBuffer = resources->createUploadBuffer(&zeroCB, (sizeof(SkyboxCB) + 255) & ~255, "SkyboxCB");

	if (!constantBuffer) return false;

	constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&cbData));
	return true;
}

bool SkyboxRenderer::createRootSignature(ID3D12Device* device) {
	CD3DX12_ROOT_PARAMETER params[3];
	params[0].InitAsConstantBufferView(0);

	CD3DX12_DESCRIPTOR_RANGE srvRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	params[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_DESCRIPTOR_RANGE sampRange;
	sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);
	params[2].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> err;
	if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
		LOG("SkyboxRenderer: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
		return false;
	}

	return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

bool SkyboxRenderer::createPipeline(ID3D12Device* device, bool useMSAA) {
	auto vs = DX::ReadData(L"SkyboxVS.cso");
	auto ps = DX::ReadData(L"SkyboxPS.cso");

	D3D12_INPUT_ELEMENT_DESC layout = {
	 "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = { vs.data(), vs.size() };
	psoDesc.PS = { ps.data(), ps.size() };
	psoDesc.InputLayout = { &layout, 1 };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = useMSAA ? 4u : 1u;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FrontCounterClockwise = TRUE;

	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

void SkyboxRenderer::render(ID3D12GraphicsCommandList* cmd, const EnvironmentMap& env, const Matrix& view, const Matrix& projection) {
	if (!env.isValid()) return;

	Matrix viewNoTranslation = view;
	viewNoTranslation._41 = 0.0f;
	viewNoTranslation._42 = 0.0f;
	viewNoTranslation._43 = 0.0f;

	cbData->vp = (viewNoTranslation * projection).Transpose();

	auto* samplers = app->getSamplerHeap();

	ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
	cmd->SetDescriptorHeaps(2, heaps);

	cmd->SetGraphicsRootSignature(rootSignature.Get());
	cmd->SetPipelineState(pso.Get());
	cmd->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(1, env.getGPUHandle());
	cmd->SetGraphicsRootDescriptorTable(2, samplers->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(vertexCount, 1, 0, 0);
}