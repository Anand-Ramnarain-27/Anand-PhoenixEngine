#include "Globals.h"
#include "API/Phoenix_Time.h"

namespace Phoenix {
    float Time::deltaTime      = 0.f;
    float Time::timeSinceStart = 0.f;
    float Time::fps            = 0.f;
    int   Time::frameCount     = 0;
}
