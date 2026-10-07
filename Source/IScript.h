#pragma once
#include <cstdint>
#include <string>
#include <vector>

class GameObject;

// Serializable script fields. A script lists the members it wants exposed from GetFields(); ComponentScript
// draws them in the Inspector, saves them into the scene/prefab JSON ("Fields") and writes them back before
// Start(). The script DLL never touches ImGui or JSON for these - the engine does both.
// Keep this file identical to ashfall's GameScript/IScript.h: it is part of the script DLL ABI.
// GameScript declares fields with the PHX_* macros (GameScript/ScriptMacros.h, docs/SCRIPT_FIELDS.md).
enum class ScriptFieldType : int {
    Int,       // int*
    Float,     // float*
    Bool,      // bool*
    String,    // std::string*
    Vec3,      // float[3] (a Vector3)
    Scene,     // std::string* - scene name, e.g. "AF_KraugsDen" (a file in Library/Scenes/)
    Prefab,    // std::string* - prefab name, e.g. "Sera" (a file in Library/Prefabs/)
    Color,     // float[4] rgba (a Vector4)
    Enum,      // int* - the index into ScriptField::enumNames
    AssetPath, // std::string* - a file under Assets/ (ScriptField::assetFilter says where and which extension)
    ObjectRef, // ScriptObjectRef* - another GameObject in the same scene, saved by its hierarchy path
};

enum ScriptFieldFlag : unsigned {
    ScriptFieldFlag_Hidden = 1u << 0,           // saved and loaded, not drawn
    ScriptFieldFlag_ReadOnly = 1u << 1,         // drawn greyed out (live while playing), never saved
    // A String holding a JSON object of per-instance overrides over the .json file named by the AssetPath field
    // `link` (the Inspector draws that file with the overrides in bold). Usually also Hidden.
    ScriptFieldFlag_ConfigOverrides = 1u << 2,
};

// An ObjectRef field's member. The engine fills `object` before Start() (and while editing); `path` is what is
// saved: names from the scene root, '/'-separated ("VFX_TestRoot/VFX_Camera").
struct ScriptObjectRef {
    std::string path;
    GameObject* object = nullptr;
};

struct ScriptField {
    const char* name = nullptr;   // key in the saved JSON - renaming it drops previously saved values
    ScriptFieldType type = ScriptFieldType::Int;
    void* value = nullptr;        // points into the script instance; see ScriptFieldType for the pointee
    const char* tooltip = nullptr;

    // ---- attributes (all optional; zero = off)
    unsigned flags = 0;                   // ScriptFieldFlag bits
    float rangeMin = 0.f, rangeMax = 0.f; // Int/Float: a slider clamped to [min, max] when min < max
    const char* header = nullptr;         // bold caption drawn above the field
    float space = 0.f;                    // pixels of space above the field (and its header)
    int multiline = 0;                    // String: rows of a multi-line text box
    const char* enumNames = nullptr;      // Enum: "Idle|Patrol|Chase" (the value is the index)
    const char* assetFilter = nullptr;    // AssetPath: "Assets/Enemies|Assets/EnemyConfigs;.json" (folders;extension)
    const char* link = nullptr;           // ConfigOverrides: the name of the AssetPath field it overrides
    const void* defaultValue = nullptr;   // same pointee type as `value`; a value equal to it isn't saved
    int arraySize = 0;                    // > 0: a fixed array of that many elements starting at `value`
};

using ScriptFieldList = std::vector<ScriptField>;

class IScript {
public:
    virtual ~IScript() = default;

    virtual void Start(GameObject* owner){}

    virtual void Update(float dt){}

    virtual bool shouldTickAI(bool isVisible, float distanceToCamera,
        float aiCullDistance, int aiCullTickRate){
        return true;
    }

    virtual void Destroy(){}

    virtual void Editor(){}

    virtual std::string Save() const { return "{}"; }

    virtual void Load(const std::string& json){}

    virtual const char* getTypeName() const = 0;

    // Append this script's serializable fields to `out` (see ScriptField). Called whenever the engine needs
    // them, so the pointers only have to stay valid for the duration of the call.
    virtual void GetFields(ScriptFieldList& out){}
};
