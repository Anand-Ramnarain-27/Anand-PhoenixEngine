#pragma once

// ============================================================
//  PhoenixAPI.h  --  single include for all Phoenix scripts
//
//  Usage in your script .h:
//      #include "PhoenixAPI.h"
//
//  Gives you:
//    Phoenix::Input    - keyboard / mouse
//    Phoenix::Time     - deltaTime, fps, timeSinceStart
//    Phoenix::Debug    - Log, LogWarning, LogError, LogFormat
//    Phoenix::Scene    - Find, Spawn, Destroy
//    Phoenix::Vec3/Quat/Color/Mat4  - math types
//    Phoenix::Key / Phoenix::MouseButton  - input enums
//    GetTransform / GetRigidbody / GetAnimation / Position ...
// ============================================================

#include "API/Phoenix_Types.h"
#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Debug.h"
#include "API/Phoenix_Time.h"
#include "API/Phoenix_Input.h"
#include "API/Phoenix_Scene.h"
#include "API/Phoenix_GameObject.h"
#include "IScript.h"
#include "ScriptExport.h"

// Pull math types into the global namespace so scripts can write
// Vec3, Quat etc. without the Phoenix:: prefix.
using Phoenix::Vec2;
using Phoenix::Vec3;
using Phoenix::Vec4;
using Phoenix::Quat;
using Phoenix::Mat4;
using Phoenix::Color;
using Phoenix::Key;
using Phoenix::MouseButton;
using Phoenix::GamepadButton;
using Phoenix::GamepadAxis;
using Phoenix::Input;
using Phoenix::Time;
using Phoenix::Debug;
using Phoenix::Scene;
using Phoenix::GetTransform;
using Phoenix::GetRigidbody;
using Phoenix::GetAnimation;
using Phoenix::GetParticles;
using Phoenix::SetActive;
using Phoenix::IsActive;
using Phoenix::GetName;
using Phoenix::Position;
using Phoenix::Scale;
using Phoenix::Rotation;
