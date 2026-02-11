#pragma once
#include "UID.h"

struct MaterialBinary
{
    UID materialUID;

    float baseColor[4];
    uint32_t hasTexture;

    UID textureUID; // 0 if none
};
