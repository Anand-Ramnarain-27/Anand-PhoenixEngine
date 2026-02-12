#pragma once
#include <string>

class SceneImporter
{
public:
    struct ImportResult
    {
        std::string meshesPath;
        std::string materialsPath;
        bool success = false;
    };

    static ImportResult ImportScene(const char* sourceFile);
};
