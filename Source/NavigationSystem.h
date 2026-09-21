#pragma once
#include "INavProvider.h"
#include "WaypointGraphProvider.h"
#include <memory>
#include <string>
#include <unordered_map>

// Owns the active scene's path-source backend and exposes it to ComponentAIAgent
// (and, later, player movement code) as a single INavProvider seam. Mirrors how
// CollisionSystem owns a swappable IBroadPhase.
class NavigationSystem {
public:
    NavigationSystem() = default;

    bool LoadWaypointGraph(const std::string& path);

    bool FindPath(const Vector3& start, const Vector3& end, std::vector<Vector3>& outPath,
                  const AgentProfile& profile = {});
    bool IsWalkable(const Vector3& point, const AgentProfile& profile = {}) const;

    INavProvider* getActiveProvider() const { return m_activeProvider.get(); }
    const char* getActiveProviderName() const { return m_activeProvider ? m_activeProvider->getName() : "None"; }

    // Debug-panel helpers - phase 1 only has WaypointGraphProvider, so these
    // dynamic_cast down to it. A future backend just returns 0 here.
    int getDebugNodeCount() const;
    int getDebugEdgeCount() const;

    void drawDebug() const;

    // Named graphs, alongside the single "active" one above - lets multiple
    // point-graphs coexist (ground patrol, climb anchors, burrow islands,
    // reveal points) without any one of them being "the" nav provider.
    bool LoadNamedGraph(const std::string& name, const std::string& path);
    bool FindPathNamed(const std::string& name, const Vector3& start, const Vector3& end,
                       std::vector<Vector3>& outPath, const AgentProfile& profile = {});
    bool IsWalkableNamed(const std::string& name, const Vector3& point,
                         const AgentProfile& profile = {}) const;

private:
    std::unique_ptr<INavProvider> m_activeProvider;
    std::unordered_map<std::string, std::unique_ptr<INavProvider>> m_namedProviders;
};
