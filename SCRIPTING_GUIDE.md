# Phoenix Engine — Scripting Guide

Scripts in Phoenix Engine are compiled C++ classes that live in the **GameScript** project. They are built as a DLL and hot-reloaded by the engine at runtime — you can recompile while the editor is open and your changes take effect immediately without restarting.

---

## Creating a Script

Right-click empty space in the **Asset Browser** (Scripts tab) and choose **Create Script…**

Enter a name (e.g. `PlayerController`) and click **Create**. Two files are generated in `Source/GameScript/` and opened in Visual Studio:

- `PlayerController.h`
- `PlayerController.cpp`

The files are also automatically registered in `GameScript.vcxproj`.

---

## Script Structure

Every script inherits from `IScript` and includes one header:

```cpp
// PlayerController.h
#pragma once
#include "PhoenixAPI.h"

class SCRIPT_API PlayerController : public IScript {
public:
    void Start  (GameObject* owner) override;
    void Update (float dt)          override;
    void Destroy()                  override;
    const char* getTypeName() const override { return "PlayerController"; }

private:
    GameObject* m_owner = nullptr;
};

extern "C" SCRIPT_API IScript* Create_PlayerController();
```

```cpp
// PlayerController.cpp
#include "PlayerController.h"

void PlayerController::Start(GameObject* owner){
    m_owner = owner;
}

void PlayerController::Update(float dt){
    // your logic here
}

void PlayerController::Destroy(){
}

IScript* Create_PlayerController(){ return new PlayerController(); }
```

### Lifecycle

| Method | When it runs |
|--------|-------------|
| `Start(GameObject* owner)` | Once, when the script is first activated. Store `owner` — it's your handle to everything. |
| `Update(float dt)` | Every frame while the scene is playing. `dt` is seconds since last frame. |
| `Destroy()` | When the GameObject is destroyed. Clean up anything you allocated. |
| `Editor()` *(optional)* | Every frame in editor mode. Use `ImGui::` calls here to add inspector UI. |
| `Save() / Load()` *(optional)* | Serialise script state to/from a JSON string for scene saving. |

---

## Attaching a Script

1. Select a GameObject in the Hierarchy.
2. In the Inspector, click **Add Component → Script**.
3. Choose your script class from the dropdown.

---

## The API

`#include "PhoenixAPI.h"` gives you everything below — no other includes needed.

---

### `Time` — Frame timing

```cpp
Time::deltaTime       // float — seconds since last frame
Time::timeSinceStart  // float — total seconds since engine launched
Time::fps             // float — current frames per second
Time::frameCount      // int   — total frames rendered
```

**Example**
```cpp
float elapsed = Time::timeSinceStart;
float speed   = 5.0f * Time::deltaTime;
```

---

### `Input` — Keyboard

```cpp
Input::IsKeyDown    (Key k)   // true while key is held
Input::IsKeyPressed (Key k)   // true only on the first frame the key goes down
Input::IsKeyReleased(Key k)   // true only on the first frame the key comes up
```

**Common keys**
```
Key::A  Key::B  ...  Key::Z
Key::D0 ... Key::D9
Key::Space   Key::Escape  Key::Enter  Key::Tab
Key::Left    Key::Right   Key::Up     Key::Down
Key::F1 ... Key::F12
Key::LeftShift   Key::RightShift
Key::LeftControl Key::RightControl
Key::LeftAlt     Key::RightAlt
```

**Example**
```cpp
if (Input::IsKeyPressed(Key::Space))
    Debug::Log("Jump!");

if (Input::IsKeyDown(Key::Escape))
    Scene::Find("GameManager")->setActive(false);
```

---

### `Input` — Mouse

```cpp
Input::IsMouseDown    (MouseButton btn)
Input::IsMousePressed (MouseButton btn)
Input::IsMouseReleased(MouseButton btn)
Input::GetMousePosition()   // Vec2 — screen pixel coords
Input::GetMouseDelta()      // Vec2 — pixels moved this frame
```

**Buttons:** `MouseButton::Left`, `MouseButton::Right`, `MouseButton::Middle`

**Example**
```cpp
if (Input::IsMousePressed(MouseButton::Left)){
    Vec2 pos = Input::GetMousePosition();
    Debug::LogFormat("Clicked at %.0f, %.0f", pos.x, pos.y);
}

Vec2 look = Input::GetMouseDelta() * 0.1f;
Rotation(m_owner) *= Quat::CreateFromYawPitchRoll(look.x, look.y, 0);
```

---

### `Input` — Axes

Returns a value in the range **−1 to 1**, combining keyboard and left gamepad stick automatically.

```cpp
float h = Input::GetAxis("Horizontal");  // A/Left=-1  D/Right=+1
float v = Input::GetAxis("Vertical");    // S/Down=-1  W/Up=+1
```

**Example**
```cpp
void PlayerController::Update(float dt){
    float h = Input::GetAxis("Horizontal");
    float v = Input::GetAxis("Vertical");
    Position(m_owner) += Vec3(h, 0.f, v) * 5.f * dt;
}
```

---

### `Input` — Gamepad / Controller

Supports up to 4 players (index 0–3). Defaults to player 0.

```cpp
Input::IsGamepadConnected(int player = 0)

// Buttons
Input::IsButtonDown    (GamepadButton btn, int player = 0)
Input::IsButtonPressed (GamepadButton btn, int player = 0)
Input::IsButtonReleased(GamepadButton btn, int player = 0)

// Analogue axes  (-1..1 for sticks, 0..1 for triggers)
Input::GetGamepadAxis(GamepadAxis axis, int player = 0)

// Rumble / vibration  (0..1)
Input::SetVibration(float leftMotor, float rightMotor, int player = 0)
```

**Buttons**
```
GamepadButton::A   GamepadButton::B   GamepadButton::X   GamepadButton::Y
GamepadButton::LeftShoulder   GamepadButton::RightShoulder
GamepadButton::LeftStick      GamepadButton::RightStick
GamepadButton::DPadUp         GamepadButton::DPadDown
GamepadButton::DPadLeft       GamepadButton::DPadRight
GamepadButton::Start          GamepadButton::Back
```

**Axes**
```
GamepadAxis::LeftStickX   GamepadAxis::LeftStickY
GamepadAxis::RightStickX  GamepadAxis::RightStickY
GamepadAxis::LeftTrigger  GamepadAxis::RightTrigger
```

**Example**
```cpp
void PlayerController::Update(float dt){
    if (!Input::IsGamepadConnected()) return;

    float moveX = Input::GetGamepadAxis(GamepadAxis::LeftStickX);
    float moveZ = Input::GetGamepadAxis(GamepadAxis::LeftStickY);
    Position(m_owner) += Vec3(moveX, 0.f, moveZ) * 5.f * dt;

    float lookX = Input::GetGamepadAxis(GamepadAxis::RightStickX);
    Rotation(m_owner) *= Quat::CreateFromYawPitchRoll(lookX * dt * 2.f, 0, 0);

    if (Input::IsButtonPressed(GamepadButton::A))
        GetRigidbody(m_owner)->velocity.y = 6.f;  // jump

    float aim = Input::GetGamepadAxis(GamepadAxis::RightTrigger);
    Input::SetVibration(0.f, aim * 0.5f);  // right motor rumbles when aiming
}
```

---

### `Debug` — Logging

Output appears in the engine Console panel.

```cpp
Debug::Log("Hello from script");
Debug::LogWarning("Something looks off");
Debug::LogError("This should not happen");
Debug::LogFormat("Health: %d / %d", hp, maxHp);
Debug::LogFormat("Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
```

---

### `Scene` — Finding and spawning objects

```cpp
GameObject* obj  = Scene::Find("EnemyBase");   // search active scene by name
GameObject* cube = Scene::Spawn("NewCube");     // create empty object
Scene::Destroy(someObject);                     // mark for destruction
```

**Example**
```cpp
void EnemyScript::Start(GameObject* owner){
    m_owner  = owner;
    m_player = Scene::Find("Player");
}

void EnemyScript::Update(float dt){
    if (!m_player) return;
    Vec3 dir = Position(m_player) - Position(m_owner);
    dir.Normalize();
    Position(m_owner) += dir * 2.f * dt;
}
```

---

### `GameObject` helpers

These are free functions — no need to call `getComponent` manually.

```cpp
GetTransform(go)   // ComponentTransform*
GetRigidbody(go)   // ComponentRigidbody*
GetAnimation(go)   // ComponentAnimation*
GetParticles(go)   // ComponentParticleSystem*

Position(go)       // Vec3& — shorthand for GetTransform(go)->position
Rotation(go)       // Quat& — shorthand for GetTransform(go)->rotation
Scale(go)          // Vec3& — shorthand for GetTransform(go)->scale

SetActive(go, bool)
IsActive(go)
GetName(go)        // const char*
```

All helpers are null-safe — if `go` is null, they return null / do nothing.

---

### Math types

| Type | Usage |
|------|-------|
| `Vec2` | 2D vector `{x, y}` |
| `Vec3` | 3D vector `{x, y, z}` |
| `Vec4` | 4D vector `{x, y, z, w}` |
| `Quat` | Quaternion rotation |
| `Mat4` | 4×4 matrix |
| `Color` | RGBA colour |

All types come from DirectX SimpleMath — see the [SimpleMath docs](https://github.com/microsoft/DirectXTK/wiki/SimpleMath) for the full API.

```cpp
Vec3 forward = Vec3(0, 0, 1);
Vec3 up      = Vec3::Up;

Quat rot = Quat::CreateFromYawPitchRoll(yaw, pitch, 0.f);

float dist = Vec3::Distance(Position(a), Position(b));
Vec3  dir  = (Position(target) - Position(m_owner));
dir.Normalize();
```

---

## Optional: Save & Load

If your script has state that should persist across scene saves, override `Save()` and `Load()`:

```cpp
// In your .h
std::string Save() const override;
void Load(const std::string& json) override;
```

```cpp
// In your .cpp
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

std::string PlayerController::Save() const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("health", m_health, a);
    doc.AddMember("speed",  m_speed,  a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    return buf.GetString();
}

void PlayerController::Load(const std::string& json){
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("health")) m_health = doc["health"].GetFloat();
    if (doc.HasMember("speed"))  m_speed  = doc["speed"].GetFloat();
}
```

---

## Optional: Inspector UI

Override `Editor()` to add custom controls in the Inspector panel using ImGui:

```cpp
void PlayerController::Editor(){
    ImGui::SliderFloat("Speed",  &m_speed,  0.f, 20.f);
    ImGui::SliderFloat("Health", &m_health, 0.f, 100.f);
    if (ImGui::Button("Reset")) m_health = 100.f;
}
```

---

## Building & Hot-Reload

1. Make your changes in Visual Studio.
2. **Build → Build GameScript** (or Ctrl+Shift+B with GameScript selected).
3. The engine detects the new DLL and hot-reloads automatically — no restart needed.

> **Release builds** work identically. The post-build event copies `GameScript.dll` to `Assets/Scripts/` for both Debug and Release configurations.

---

## Full example — third-person character

```cpp
#pragma once
#include "PhoenixAPI.h"

class SCRIPT_API ThirdPersonController : public IScript {
public:
    void Start  (GameObject* owner) override;
    void Update (float dt)          override;
    void Destroy()                  override;
    void Editor ()                  override;
    const char* getTypeName() const override { return "ThirdPersonController"; }

private:
    GameObject* m_owner   = nullptr;
    float       m_speed   = 5.f;
    float       m_jumpForce = 6.f;
};

extern "C" SCRIPT_API IScript* Create_ThirdPersonController();
```

```cpp
#include "ThirdPersonController.h"

void ThirdPersonController::Start(GameObject* owner){
    m_owner = owner;
}

void ThirdPersonController::Update(float dt){
    // --- Movement (keyboard + left stick) ---
    float h = Input::GetAxis("Horizontal");
    float v = Input::GetAxis("Vertical");
    Position(m_owner) += Vec3(h, 0.f, v) * m_speed * dt;

    // --- Jump (Space or gamepad A) ---
    bool jump = Input::IsKeyPressed(Key::Space)
             || Input::IsButtonPressed(GamepadButton::A);
    if (jump){
        if (auto* rb = GetRigidbody(m_owner))
            rb->velocity.y = m_jumpForce;
        Debug::Log("Jump!");
    }

    // --- Camera look (right stick) ---
    if (Input::IsGamepadConnected()){
        float lookX = Input::GetGamepadAxis(GamepadAxis::RightStickX);
        Rotation(m_owner) *= Quat::CreateFromYawPitchRoll(lookX * dt * 2.f, 0.f, 0.f);
    }

    // --- Aim vibration ---
    float aim = Input::GetGamepadAxis(GamepadAxis::LeftTrigger);
    Input::SetVibration(0.f, aim * 0.4f);
}

void ThirdPersonController::Destroy(){
    Input::SetVibration(0.f, 0.f);  // stop rumble on destroy
}

void ThirdPersonController::Editor(){
    ImGui::SliderFloat("Speed##tpc",     &m_speed,     1.f, 20.f);
    ImGui::SliderFloat("Jump Force##tpc",&m_jumpForce, 1.f, 15.f);
}

IScript* Create_ThirdPersonController(){ return new ThirdPersonController(); }
```
