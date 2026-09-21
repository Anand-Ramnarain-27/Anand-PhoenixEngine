#pragma once
#include "INavProvider.h"
#include "WaypointGraphProvider.h"
#include <memory>
#include <string>
#include <unordered_map>

// Owns the scene's path-source backend(s), mirrors how CollisionSystem owns
// a swappable IBroadPhase.
class NavigationSystem {
public:
    NavigationSystem() = default;

    bool LoadWaypointGraph(const std::string& path);

    bool FindPath(const Vector3& start, const Vector3& end, std::vector<Vector3>& outPath,
                  const AgentProfile& profile = {});
    bool IsWalkable(const Vector3& point, const AgentProfile& profile = {}) const;

    INavProvider* getActiveProvider() const { return m_activeProvider.get(); }
    const char* getActiveProviderName() const { return m_activeProvider ? m_activeProvider->getName() : "None"; }

    int getDebugNodeCount() const;
    int getDebugEdgeCount() const;

    void drawDebug() const;

    // Additional graphs alongside the active one (climb anchors, burrow islands, etc).
    bool LoadNamedGraph(const std::string& name, const std::string& path);
    bool FindPathNamed(const std::string& name, const Vector3& start, const Vector3& end,
                       std::vector<Vector3>& outPath, const AgentProfile& profile = {});
    bool IsWalkableNamed(const std::string& name, const Vector3& point,
                         const AgentProfile& profile = {}) const;

private:
    std::unique_ptr<INavProvider> m_activeProvider;
    std::unordered_map<std::string, std::unique_ptr<INavProvider>> m_namedProviders;
};
