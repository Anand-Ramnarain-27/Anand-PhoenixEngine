#pragma once
#include <string>
#include <vector>

class GameObject;

// Serializable script fields. A script lists the members it wants exposed from GetFields(); ComponentScript
// draws them in the Inspector, saves them into the scene/prefab JSON ("Fields") and writes them back before
// Start(). The script DLL never touches ImGui or JSON for these - the engine does both.
// Keep this file identical to ashfall's GameScript/IScript.h: it is part of the script DLL ABI.
enum class ScriptFieldType : int {
    Int,     // int*
    Float,   // float*
    Bool,    // bool*
    String,  // std::string*
    Vec3,    // float[3] (a Vector3)
    Scene,   // std::string* - scene name, e.g. "AF_KraugsDen" (a file in Library/Scenes/)
    Prefab,  // std::string* - prefab name, e.g. "Sera" (a file in Library/Prefabs/)
};

struct ScriptField {
    const char* name = nullptr;   // key in the saved JSON - renaming it drops previously saved values
    ScriptFieldType type = ScriptFieldType::Int;
    void* value = nullptr;        // points into the script instance; see ScriptFieldType for the pointee
    const char* tooltip = nullptr;
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
