#include "Globals.h"
#include "ResourceAnimation.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <cstring>

bool ResourceAnimation::LoadInMemory() {
    return true;
}

void ResourceAnimation::UnloadFromMemory() {
    channels.clear();
    morphChannels.clear();
    duration = 0.f;
}
