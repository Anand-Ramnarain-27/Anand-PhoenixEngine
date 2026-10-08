#pragma once
// Precompiled header for every engine translation unit: Windows, D3D12, SimpleMath, logging, debug draw, ImGui
// and the engine-wide constants. Include it first.

#define NOMINMAX
#define INITGUID

#ifdef _DEBUG
#define USE_PIX 1
#else
#define USE_PIX 0
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include "d3dx12.h"

#include "SimpleMath.h"

#if USE_PIX
#include "WinPixEventRuntime/pix3.h"
#endif

#include <assert.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

#include "PhoenixLog.h"

/// Uncategorised debugger output. Engine code logs through PHX_LOG; this stays for Phoenix::Debug (script API).
void log(const char file[], int line, const char* format, ...);

/// Frames the CPU may record ahead of the GPU; per-frame upload rings are sized by it.
#define FRAMES_IN_FLIGHT 3

/// HDR scene colour, before tonemapping.
static constexpr DXGI_FORMAT kSceneColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

#include "debug_draw.hpp"
inline const ddVec3& ddConvert(const Vector3& v){ return reinterpret_cast<const ddVec3&>(v); }
inline const ddMat4x4& ddConvert(const Matrix& m){ return reinterpret_cast<const ddMat4x4&>(m); }

inline size_t alignUp(size_t value, size_t alignment){
    return (value + alignment - 1) & ~(alignment - 1);
}

#if USE_PIX
#define BEGIN_EVENT(commandList, text) PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, text)
#define END_EVENT(commandList) PIXEndEvent(commandList)
#define SET_MARKER(commandList, text) PIXSetMarker(commandList, PIX_COLOR_DEFAULT, text)
#else
#define BEGIN_EVENT(commandList, text)
#define END_EVENT(commandList)
#define SET_MARKER(commandList, text)
#endif

#include <imgui.h>
#include <imgui_internal.h>
