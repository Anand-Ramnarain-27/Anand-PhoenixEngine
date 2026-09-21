#pragma once
#include "Globals.h"

enum class UIEventType {
    HoverEnter = 0,
    HoverExit,
    Press,
    Release,
    Click,
    ValueChanged,
    Count
};

// Pointer/keyboard state for one UI update. The pointer is in pixels of the render target the UI is drawn into.
struct UIInput {
    bool pointerValid = false;
    Vector2 pointer = Vector2::Zero;
    bool mousePressed = false;
    bool mouseReleased = false;

    bool tabPressed = false;
    bool shiftDown = false;
    bool submitPressed = false;
    bool submitReleased = false;

    // Arrow keys pressed this frame: -1 / 0 / +1 (Right and Up are positive). Used to nudge focused sliders.
    int navX = 0;
    int navY = 0;
};
