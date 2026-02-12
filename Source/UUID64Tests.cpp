#include "Globals.h"
#include "UUID64Tests.h"
#include "UUID64.h"
#include "GameObject.h"
#include "ModuleScene.h"
#include <string>
#include <unordered_map>
#include <sstream>

// We don't include ModuleEditor.h here to avoid circular dependencies
// Instead, we'll use a forward declaration and runtime check
class ModuleEditor;

// Helper function for logging - will be called from editor context
static void EditorLog(ModuleEditor* editor, const std::string& text, const ImVec4& color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f))
{
    // This function should be implemented in ModuleEditor.cpp
    // We're just declaring it here - the actual implementation will be linked
    extern void LogToEditor(ModuleEditor * editor, const std::string & text, const ImVec4 & color);

    if (editor)
    {
        LogToEditor(editor, text, color);
    }
    else
    {
        // Fallback to console output
        OutputDebugStringA(text.c_str());
        OutputDebugStringA("\n");
    }
}

void TestUUIDGeneration(ModuleEditor* editor = nullptr)
{
    EditorLog(editor, "\n=== UUID64 Generation Test ===", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

    UUID64 uuid1 = UUID64::Generate();
    UUID64 uuid2 = UUID64::Generate();
    UUID64 uuid3 = UUID64::Generate();

    EditorLog(editor, "UUID64 1: " + uuid1.toString());
    EditorLog(editor, "UUID64 2: " + uuid2.toString());
    EditorLog(editor, "UUID64 3: " + uuid3.toString());

    if (uuid1 != uuid2 && uuid2 != uuid3 && uuid1 != uuid3)
    {
        EditorLog(editor, "? All UUID64s are unique", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
    else
    {
        EditorLog(editor, "? COLLISION DETECTED! (This should never happen)", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }

    UUID64 invalid = UUID64::Invalid;
    EditorLog(editor, "Invalid UUID64: " + invalid.toString());
    EditorLog(editor, "Is valid? " + std::string(invalid.isValid() ? "yes" : "no"));
}

void TestGameObjectUUID(ModuleEditor* editor = nullptr)
{
    EditorLog(editor, "\n=== GameObject UUID64 Test ===", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

    GameObject go1("Player");
    GameObject go2("Enemy");
    GameObject go3("Pickup");

    EditorLog(editor, "GameObject '" + go1.getName() + "' UUID64: " + go1.getUUID().toString());
    EditorLog(editor, "GameObject '" + go2.getName() + "' UUID64: " + go2.getUUID().toString());
    EditorLog(editor, "GameObject '" + go3.getName() + "' UUID64: " + go3.getUUID().toString());

    if (go1.getUUID().isValid() && go2.getUUID().isValid() && go3.getUUID().isValid())
    {
        EditorLog(editor, "? All GameObject UUID64s are valid", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }

    if (go1.getUUID() != go2.getUUID() &&
        go2.getUUID() != go3.getUUID() &&
        go1.getUUID() != go3.getUUID())
    {
        EditorLog(editor, "? All GameObject UUID64s are unique", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
}

void TestSceneUUIDLookup(ModuleEditor* editor = nullptr)
{
    EditorLog(editor, "\n=== Scene UUID64 Lookup Test ===", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

    ModuleScene scene;

    GameObject* player = scene.createGameObject("Player");
    GameObject* enemy1 = scene.createGameObject("Enemy1");
    GameObject* enemy2 = scene.createGameObject("Enemy2");

    EditorLog(editor, "Created 3 GameObjects:");
    EditorLog(editor, "  Player: " + player->getUUID().toString());
    EditorLog(editor, "  Enemy1: " + enemy1->getUUID().toString());
    EditorLog(editor, "  Enemy2: " + enemy2->getUUID().toString());

    EditorLog(editor, "Testing UUID64 lookup...");

    GameObject* found1 = scene.findGameObjectByUUID(player->getUUID());
    GameObject* found2 = scene.findGameObjectByUUID(enemy1->getUUID());
    GameObject* found3 = scene.findGameObjectByUUID(enemy2->getUUID());

    if (found1 == player && found2 == enemy1 && found3 == enemy2)
    {
        EditorLog(editor, "? UUID64 lookup works correctly", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        EditorLog(editor, "  Found Player: " + found1->getName());
        EditorLog(editor, "  Found Enemy1: " + found2->getName());
        EditorLog(editor, "  Found Enemy2: " + found3->getName());
    }
    else
    {
        EditorLog(editor, "? UUID64 lookup failed!", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }

    UUID64 fakeUUID = UUID64::Generate();
    GameObject* notFound = scene.findGameObjectByUUID(fakeUUID);

    if (notFound == nullptr)
    {
        EditorLog(editor, "? Lookup with non-existent UUID64 correctly returns nullptr", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
}

void TestUUIDInHashMap(ModuleEditor* editor = nullptr)
{
    EditorLog(editor, "\n=== UUID64 HashMap Test ===", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

    std::unordered_map<UUID64, std::string> uuidToName;

    GameObject go1("Alice");
    GameObject go2("Bob");
    GameObject go3("Charlie");

    uuidToName[go1.getUUID()] = go1.getName();
    uuidToName[go2.getUUID()] = go2.getName();
    uuidToName[go3.getUUID()] = go3.getName();

    EditorLog(editor, "Stored 3 GameObjects in unordered_map<UUID64, string>");

    std::string name1 = uuidToName[go1.getUUID()];
    std::string name2 = uuidToName[go2.getUUID()];
    std::string name3 = uuidToName[go3.getUUID()];

    if (name1 == "Alice" && name2 == "Bob" && name3 == "Charlie")
    {
        EditorLog(editor, "? UUID64 hash function works correctly", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        EditorLog(editor, "  Retrieved: " + name1 + ", " + name2 + ", " + name3);
    }
}

void RunUUID64Tests(ModuleEditor* editor)
{
    TestUUIDGeneration(editor);
    TestGameObjectUUID(editor);
    TestSceneUUIDLookup(editor);
    TestUUIDInHashMap(editor);

    EditorLog(editor, "\n========================================", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    EditorLog(editor, "       ALL TESTS COMPLETE", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    EditorLog(editor, "========================================\n", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
}

void RunUUID64Tests()
{
    RunUUID64Tests(nullptr);
}