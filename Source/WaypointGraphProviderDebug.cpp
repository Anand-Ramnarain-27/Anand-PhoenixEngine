// drawDebug() split out from WaypointGraphProvider.cpp so PhoenixCore/
// GameScript.dll (which link that file for Navigation::FindPath) don't have
// to pull in debug_draw.hpp. Engine/Player only, same as CollisionDebugPanel
// and other debug-draw call sites.
#include "Globals.h"
#include "WaypointGraphProvider.h"
#include <algorithm>

void WaypointGraphProvider::drawDebug() const{
    for (const auto& n : getNodes()){
        dd::sphere(ddConvert(n.position), dd::colors::Orange, 0.25f);
        for (int neighborId : n.neighborIds){
            if (neighborId <= n.id) continue; // draw each edge once
            auto it = std::find_if(getNodes().begin(), getNodes().end(),
                [&](const Node& o){ return o.id == neighborId; });
            if (it != getNodes().end())
                dd::line(ddConvert(n.position), ddConvert(it->position), dd::colors::Orange);
        }
    }
}
