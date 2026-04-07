#include "Globals.h"
#include "ResourceAnimation.h"
#include "AnimationImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <algorithm>

ResourceAnimation::ResourceAnimation(UID uid)
    : ResourceBase(uid, Type::Unknown) {
}

ResourceAnimation::~ResourceAnimation() { UnloadFromMemory(); }

bool ResourceAnimation::LoadInMemory() {
    if (libraryFile.empty()) return true; 
    return AnimationImporter::Load(libraryFile, *this, app->getFileSystem());
}

void ResourceAnimation::UnloadFromMemory() {
    m_channels.clear();
    m_morphChannels.clear();
    m_duration = 0.0f;
}

void ResourceAnimation::addChannel(const std::string& name, Channel ch) {
    m_channels[name] = std::move(ch);
}

void ResourceAnimation::addMorphChannel(const std::string& name, MorphChannel mc) {
    m_morphChannels[name] = std::move(mc);
}

void ResourceAnimation::computeDuration() {
    m_duration = 0.0f;
    for (const auto& [name, ch] : m_channels) {
        if (ch.numPositions > 0)
            m_duration = std::max(m_duration, ch.posTimeStamps[ch.numPositions - 1]);
        if (ch.numRotations > 0)
            m_duration = std::max(m_duration, ch.rotTimeStamps[ch.numRotations - 1]);
    }
    for (const auto& [name, mc] : m_morphChannels) {
        if (mc.numTime > 0)
            m_duration = std::max(m_duration, mc.weightsTimes[mc.numTime - 1]);
    }
}
