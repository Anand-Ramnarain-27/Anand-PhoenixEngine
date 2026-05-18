#include "Globals.h"
#include "ResourceAnimation.h"
#include "AnimationImporter.h"   // we'll write this next

bool ResourceAnimation::LoadInMemory() {
    if (!m_channels.empty()) return true;  // already loaded
    return AnimationImporter::Load(libraryFile, *this);
}

void ResourceAnimation::UnloadFromMemory() {
    m_channels.clear();
    m_duration = 0.0f;
}

const AnimChannel* ResourceAnimation::getChannel(
    const std::string& name) const
{
    auto it = m_channels.find(name);
    return (it != m_channels.end()) ? &it->second : nullptr;
}

void ResourceAnimation::recomputeDuration() {
    m_duration = 0.0f;
    for (const auto& [name, ch] : m_channels) {
        if (!ch.posTimestamps.empty())
            m_duration = std::max(m_duration, ch.posTimestamps.back());
        if (!ch.rotTimestamps.empty())
            m_duration = std::max(m_duration, ch.rotTimestamps.back());
        if (!ch.scaleTimestamps.empty())
            m_duration = std::max(m_duration, ch.scaleTimestamps.back());
    }
}