#pragma once
#include "UID.h"

class TextureImporter
{
public:
    static UID Import(
        const char* sourcePath,
        const char* libraryPath
    );
};
