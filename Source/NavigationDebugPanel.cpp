#include "Globals.h"
#include "NavigationDebugPanel.h"
#include "ModuleEditor.h"
#include "NavigationSystem.h"
#include "EditorSceneSettings.h"
#include "SceneManager.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentAIAgent.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <functional>

namespace {
    void gatherAgents(GameObject* node, std::vector<GameObject*>& out){
        if (!node) return;
        if (node->getComponent<ComponentAIAgent>()) out.push_back(node);
        for (GameObject* child : node->getChildren()) gatherAgents(child, out);
    }
}

void NavigationDebugPanel::drawContent(){
    NavigationSystem* nav = m_editor->getNavigationSystem();
    if (!nav){ textMuted("No navigation system."); return; }

    EditorSceneSettings* s = m_editor->getSceneManager() ? &m_editor->getSceneManager()->getSettings() : nullptr;

    ImGui::Text("Active provider: %s", nav->getActiveProviderName());
    ImGui::Text("Nodes: %d   Edges: %d", nav->getDebugNodeCount(), nav->getDebugEdgeCount());

    if (ImGui::Button("Load Test Graph")){
        std::string path = app->getFileSystem()->GetAssetsPath() + "Navigation/TestWaypointGraph.json";
        if (!nav->LoadWaypointGraph(path))
            textDanger("Failed to load '%s' - see console.", path.c_str());
    }

    if (s) ImGui::Checkbox("Draw Navigation Graph", &s->debugDrawNav);

    ImGui::SeparatorText("Agents");

    SceneGraph* scene = m_editor->getActiveModuleScene();
    std::vector<GameObject*> agents;
    if (scene) gatherAgents(scene->getRoot(), agents);

    if (agents.empty()){ textMuted("No AI agents in scene."); return; }

    static const char* behaviorNames[] = { "Idle", "Patrol", "Chase" };
    for (GameObject* go : agents){
        ComponentAIAgent* agent = go->getComponent<ComponentAIAgent>();
        if (!agent) continue;
        ImGui::Text("%s  -  %s  (%d waypoints)", go->getName().c_str(),
            behaviorNames[(int)agent->getBehavior()], (int)agent->getCurrentPath().size());
    }
}
