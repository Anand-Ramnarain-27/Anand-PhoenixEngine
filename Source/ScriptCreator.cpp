#include "Globals.h"
#include "ScriptCreator.h"
#include <filesystem>
#include <string>
#include <cstdio>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Header template
// ---------------------------------------------------------------------------
bool ScriptCreator::writeHeader(const std::string& name, const std::string& path){
    std::string h =
        "#pragma once\n"
        "#include \"IScript.h\"\n"
        "#include \"ScriptExport.h\"\n"
        "\n"
        "class SCRIPT_API " + name + " : public IScript {\n"
        "public:\n"
        "    void Start(GameObject* owner) override;\n"
        "    void Update(float dt) override;\n"
        "    void Destroy() override;\n"
        "    const char* getTypeName() const override { return \"" + name + "\"; }\n"
        "\n"
        "private:\n"
        "    GameObject* m_owner = nullptr;\n"
        "};\n"
        "\n"
        "extern \"C\" SCRIPT_API IScript* Create_" + name + "();\n";

    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return false;
    fputs(h.c_str(), f);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
// Source template
// ---------------------------------------------------------------------------
bool ScriptCreator::writeSource(const std::string& name, const std::string& path){
    std::string cpp =
        "#include \"" + name + ".h\"\n"
        "#include \"Globals.h\"\n"
        "\n"
        "void " + name + "::Start(GameObject* owner){\n"
        "    m_owner = owner;\n"
        "}\n"
        "\n"
        "void " + name + "::Update(float dt){\n"
        "}\n"
        "\n"
        "void " + name + "::Destroy(){\n"
        "}\n"
        "\n"
        "IScript* Create_" + name + "(){ return new " + name + "(); }\n";

    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return false;
    fputs(cpp.c_str(), f);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
// .vcxproj patcher — inserts ClInclude / ClCompile entries
// ---------------------------------------------------------------------------
bool ScriptCreator::patchVcxproj(const std::string& name, const std::string& vcxPath){
    if (!fs::exists(vcxPath)) return false;

    FILE* f = nullptr;
    fopen_s(&f, vcxPath.c_str(), "r");
    if (!f) return false;

    std::string content;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) content += buf;
    fclose(f);

    auto insertBeforeClose = [](std::string& s, const std::string& startTag, const std::string& entry){
        size_t pos = s.find(startTag);
        if (pos == std::string::npos) return;
        size_t closePos = s.find("</ItemGroup>", pos);
        if (closePos != std::string::npos) s.insert(closePos, entry);
    };

    insertBeforeClose(content, "<ClInclude", "    <ClInclude Include=\"" + name + ".h\" />\n");
    insertBeforeClose(content, "<ClCompile", "    <ClCompile Include=\"" + name + ".cpp\" />\n");

    fopen_s(&f, vcxPath.c_str(), "w");
    if (!f) return false;
    fputs(content.c_str(), f);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
ScriptCreator::Result ScriptCreator::create(const std::string& name, const std::string& gameScriptDir){
    Result r;
    if (name.empty()){
        r.message = "Script name cannot be empty.";
        return r;
    }

    fs::path gsDir(gameScriptDir);
    r.hPath   = (gsDir / (name + ".h")).string();
    r.cppPath = (gsDir / (name + ".cpp")).string();
    std::string vcxPath = (gsDir / "GameScript.vcxproj").string();

    if (fs::exists(r.hPath) || fs::exists(r.cppPath)){
        r.existed = true;
        r.message = "Script '" + name + "' already exists.";
        return r;
    }

    bool hOk   = writeHeader(name, r.hPath);
    bool cppOk = writeSource(name, r.cppPath);
    patchVcxproj(name, vcxPath);

    r.ok      = hOk && cppOk;
    r.message = r.ok ? "Script '" + name + "' created — rebuild GameScript to compile."
                     : "Failed to write script files for '" + name + "'.";
    return r;
}
