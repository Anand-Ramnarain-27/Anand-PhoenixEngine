// drawDebug() split out from NavigationSystem.cpp so PhoenixCore/GameScript.dll
// (which link that file for Navigation::FindPath) don't transitively need
// debug_draw.hpp via WaypointGraphProvider::drawDebug(). Engine/Player only.
#include "Globals.h"
#include "NavigationSystem.h"

void NavigationSystem::drawDebug() const{
    if (auto* wp = dynamic_cast<WaypointGraphProvider*>(getActiveProvider()))
        wp->drawDebug();
}
