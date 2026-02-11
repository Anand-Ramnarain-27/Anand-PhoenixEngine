#include "Globals.h"
#include "TextureImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"

UID TextureImporter::Import(
    const char* sourcePath,
    const char* libraryPath)
{
    UID uid = GenerateUID();

    // Let ModuleResources load image once
    auto tex = app->getResources()->createTextureFromFile(
        sourcePath, true
    );

    if (!tex)
        return 0;

    std::string outPath =
        std::string(libraryPath) +
        std::to_string(uid) + ".tex";

    // For Phase 4, we assume ModuleResources
    // can dump texture data (or stub this)

    // SAVE HEADER ONLY for now (acceptable)
    TextureBinaryHeader header{};
    header.textureUID = uid;

    app->getFileSystem()->Save(
        outPath.c_str(),
        &header,
        sizeof(header)
    );

    return uid;
}
