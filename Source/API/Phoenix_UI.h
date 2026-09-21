#pragma once
#include "API/Phoenix_Types.h"
#include <cstdint>
#include <functional>
#include <string>

class GameObject;

namespace Phoenix {

using UICallback = std::function<void()>;

// Identifies one registered listener so it can be removed later. It stays safe to hold or remove after the
// button itself has been destroyed.
struct UIListener {
    uint32_t objectUid = 0;
    int event = 0;
    uint32_t id = 0;

    bool valid() const { return id != 0; }
};

// Talks to the widgets of the UI system (Canvas / Button / Image / Label).
//
// Events are delegates: register in Start(), remove in Destroy() if the script can be destroyed while the button
// lives, otherwise the callback would outlive the script. Listeners run after the frame's UI input is processed,
// so a callback may safely destroy objects, including its own button.
//
// If you would rather not use callbacks, poll: WasClicked / IsHovered / IsPressed are valid in the Update that
// follows the event.
struct UI {
    // ---- UI -> game: button events ----
    static UIListener OnClick(GameObject* button, UICallback callback);
    static UIListener OnPress(GameObject* button, UICallback callback);
    static UIListener OnRelease(GameObject* button, UICallback callback);
    static UIListener OnHoverEnter(GameObject* button, UICallback callback);
    static UIListener OnHoverExit(GameObject* button, UICallback callback);

    static void RemoveListener(const UIListener& listener);
    static void RemoveAllListeners(GameObject* button);

    // ---- UI -> game: polling ----
    static bool WasClicked(GameObject* button);
    static bool IsHovered(GameObject* button);
    static bool IsPressed(GameObject* button);
    static bool IsFocused(GameObject* button);

    // True while the pointer is over an input-blocking widget: use it to ignore clicks the UI already owns.
    static bool IsPointerOverUI();

    // ---- game -> UI ----
    static void SetInteractable(GameObject* button, bool interactable);
    static bool IsInteractable(GameObject* button);

    static void SetVisible(GameObject* widget, bool visible);
    static bool IsVisible(GameObject* widget);

    static void SetText(GameObject* label, const std::string& text);
    static std::string GetText(GameObject* label);
    static void SetTextColor(GameObject* label, Color color);

    static void SetImageTint(GameObject* image, Color color);
    static void SetImageTexture(GameObject* image, const std::string& path);
};

} // namespace Phoenix
