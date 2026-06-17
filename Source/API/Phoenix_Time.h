#pragma once

namespace Phoenix {

// Updated by the engine each frame before Update() is called.
// Read-only from scripts.
struct Time {
    static float deltaTime;       // seconds since last frame
    static float timeSinceStart;  // total seconds since engine started
    static float fps;             // frames per second (rolling average)
    static int   frameCount;      // total frames rendered
};

} // namespace Phoenix
