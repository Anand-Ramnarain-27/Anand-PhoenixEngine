#include "Globals.h"
#include "NavigationSystem.h"

bool NavigationSystem::LoadWaypointGraph(const std::string& path){
    auto provider = std::make_unique<WaypointGraphProvider>();
    if (!provider->Load(path)){
        LOG("NavigationSystem: failed to load waypoint graph '%s'", path.c_str());
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

// drawDebug() lives in NavigationSystemDebug.cpp (Engine/Player only) - it
// calls WaypointGraphProvider::drawDebug(), which needs debug_draw.hpp.

bool NavigationSystem::LoadNamedGraph(const std::string& name, const std::string& path){
    auto provider = std::make_unique<WaypointGraphProvider>();
    if (!provider->Load(path)){
        LOG("NavigationSystem: failed to load named graph '%s' from '%s'", name.c_str(), path.c_str());
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
