#pragma once
#include <string>

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
