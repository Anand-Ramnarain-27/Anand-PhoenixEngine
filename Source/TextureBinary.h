#pragma once
#include "UID.h"

struct TextureBinaryHeader
{
    UID textureUID;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t dataSize;
};
