// Split from NavigationSystem.cpp so PhoenixCore/GameScript.dll don't need
// debug_draw.hpp. Engine/Player only.
#include "Globals.h"
#include "NavigationSystem.h"

void NavigationSystem::drawDebug() const{
    if (auto* wp = dynamic_cast<WaypointGraphProvider*>(getActiveProvider()))
        wp->drawDebug();
}
