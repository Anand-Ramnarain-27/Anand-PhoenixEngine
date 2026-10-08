#pragma once
// Editor panel for the collision pipeline: phase timings, broad-phase choice and tuning, and this frame's contacts.

#include "EditorPanel.h"

class CollisionSystem;
struct CollisionResults;
struct EditorSceneSettings;

class CollisionDebugPanel : public EditorPanel {
public:
    explicit CollisionDebugPanel(ModuleEditor* editor)
        : EditorPanel(editor){ open = false; }
    const char* getName() const override { return "Collision Debug"; }

protected:
    void drawContent() override;

private:
    void drawTimingTable(const CollisionResults& r, float leftW);
    void drawDebugToggles(EditorSceneSettings* s);
    void drawBroadPhaseControls(CollisionSystem* cs);
    void drawPipelineStats(CollisionSystem* cs, const CollisionResults& r);
    void drawContacts(const CollisionResults& r);
};
