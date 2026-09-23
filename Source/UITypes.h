#pragma once
#include "Globals.h"
#include <string>

enum class UIEventType {
    HoverEnter = 0,
    HoverExit,
    Press,
    Release,
    Click,
    ValueChanged,
    Submit,
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

    // Text editing for the focused InputBox. `text` is what was typed this frame (printable ASCII), `paste` is
    // clipboard text for a Ctrl+V. Key flags include OS key-repeat.
    std::string text;
    std::string paste;
    int backspace = 0;    // presses this frame
    int deleteKey = 0;
    bool home = false;
    bool end = false;
    bool enterPressed = false;
    bool escapePressed = false;

    // Selection: Ctrl+A/Ctrl+C/Ctrl+X for the focused InputBox. Shift+click/drag/arrow/Home/End selection uses
    // the fields above (mousePressed/pointer, navX, home, end) together with `shiftDown`.
    bool selectAllPressed = false;
    bool copyPressed = false;
    bool cutPressed = false;
};
