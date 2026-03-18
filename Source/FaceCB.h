#pragma once
#include <DirectXMath.h>

struct alignas(256) FaceCB {
	DirectX::XMFLOAT4X4 vp;
	int flipX;
	int flipZ;
	float roughness;
	float pad[1];
};