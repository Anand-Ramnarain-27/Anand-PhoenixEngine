#pragma once
// Creates a new script class (.h / .cpp from a template) and adds it to GameScript.vcxproj.

#include <string>

/// Editor helper behind "Create Script": writes <name>.h / <name>.cpp from a template into the GameScript folder
/// and adds them to its .vcxproj. An existing script of that name is left alone (Result::existed).
class ScriptCreator {
public:
    struct Result {
        bool    ok      = false;
        bool    existed = false;
        std::string message;
        std::string hPath;
        std::string cppPath;
    };

    static Result create(const std::string& name, const std::string& gameScriptDir);

private:
    static bool writeHeader (const std::string& name, const std::string& path);
    static bool writeSource (const std::string& name, const std::string& path);
    static bool patchVcxproj(const std::string& name, const std::string& vcxPath);
};
