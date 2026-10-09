#include "Globals.h"
#include "NavigationSystem.h"

bool NavigationSystem::LoadWaypointGraph(const std::string& path){
    auto provider = std::make_unique<WaypointGraphProvider>();
    if (!provider->Load(path)){
        PHX_LOG(AI, Error, "NavigationSystem: failed to load waypoint graph '%s'", path.c_str());
        return false;
    }
    m_activeProvider = std::move(provider);
    return true;
}

bool NavigationSystem::FindPath(const Vector3& start, const Vector3& end, std::vector<Vector3>& outPath,
                                const AgentProfile& profile){
    if (!m_activeProvider) return false;
    return m_activeProvider->FindPath(start, end, profile, outPath);
}

bool NavigationSystem::IsWalkable(const Vector3& point, const AgentProfile& profile) const{
    if (!m_activeProvider) return false;
    return m_activeProvider->IsWalkable(point, profile);
}

int NavigationSystem::getDebugNodeCount() const{
    auto* wp = dynamic_cast<WaypointGraphProvider*>(m_activeProvider.get());
    return wp ? wp->getNodeCount() : 0;
}

int NavigationSystem::getDebugEdgeCount() const{
    auto* wp = dynamic_cast<WaypointGraphProvider*>(m_activeProvider.get());
    return wp ? wp->getEdgeCount() : 0;
}

// drawDebug() is in NavigationSystemDebug.cpp - needs debug_draw.hpp.

int NavigationSystem::SetNodesBlocked(const std::string& name, const Vector3& center, float radius, bool blocked){
    int changed = 0;
    auto apply = [&](INavProvider* p){
        if (auto* wp = dynamic_cast<WaypointGraphProvider*>(p)) changed += wp->SetNodesBlocked(center, radius, blocked);
    };
    if (name.empty()){
        apply(m_activeProvider.get());
        for (auto& kv : m_namedProviders) apply(kv.second.get());
    }
    else if (auto it = m_namedProviders.find(name); it != m_namedProviders.end()) apply(it->second.get());
    return changed;
}

bool NavigationSystem::LoadNamedGraph(const std::string& name, const std::string& path){
    auto provider = std::make_unique<WaypointGraphProvider>();
    if (!provider->Load(path)){
        PHX_LOG(AI, Error, "NavigationSystem: failed to load named graph '%s' from '%s'", name.c_str(), path.c_str());
        return false;
    }
    m_namedProviders[name] = std::move(provider);
    return true;
}

bool NavigationSystem::FindPathNamed(const std::string& name, const Vector3& start, const Vector3& end,
                                     std::vector<Vector3>& outPath, const AgentProfile& profile){
    auto it = m_namedProviders.find(name);
    if (it == m_namedProviders.end()) return false;
    return it->second->FindPath(start, end, profile, outPath);
}

bool NavigationSystem::IsWalkableNamed(const std::string& name, const Vector3& point,
                                       const AgentProfile& profile) const{
    auto it = m_namedProviders.find(name);
    if (it == m_namedProviders.end()) return false;
    return it->second->IsWalkable(point, profile);
}
