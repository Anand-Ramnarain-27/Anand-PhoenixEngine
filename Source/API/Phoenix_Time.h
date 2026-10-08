#pragma once
// Frame timing, written by Application once per frame.

namespace Phoenix {

/// Frame timing. Scripts normally use the `dt` passed to IScript::Update instead.
struct Time {
    static float deltaTime;        ///< seconds since the previous frame (unscaled)
    static float timeSinceStart;   ///< seconds since the application started
    static float fps;
    static int   frameCount;
};

} // namespace Phoenix
