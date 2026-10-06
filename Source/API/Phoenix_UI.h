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

// Talks to the widgets of the UI system (Canvas / Button / CheckBox / RadioGroup / Slider / InputBox / ProgressBar / Image / Label).
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

    // Value changes made by the user (not by SetChecked / SetSliderValue): a checkbox toggle, a slider drag or nudge.
    static UIListener OnToggled(GameObject* checkbox, std::function<void(bool checked)> callback);
    static UIListener OnValueChanged(GameObject* slider, std::function<void(float value)> callback);

    // Text fields: every user edit, and Enter. (Not raised by SetInputText.)
    static UIListener OnTextChanged(GameObject* inputBox, std::function<void(const std::string& text)> callback);
    static UIListener OnSubmit(GameObject* inputBox, std::function<void(const std::string& text)> callback);

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

    static void SetChecked(GameObject* checkbox, bool checked);
    static bool IsChecked(GameObject* checkbox);

    // The GameObject carrying the ComponentRadioGroup — every CheckBox under it (recursively, unless owned by
    // a nested group) is a member. Returns the currently-selected member's GameObject, or null if none is.
    static GameObject* GetSelectedRadioOption(GameObject* radioGroup);

    static std::string GetInputText(GameObject* inputBox);
    static void SetInputText(GameObject* inputBox, const std::string& text);   // filtered like typed input

    // True while a text field has keyboard focus: ignore game hotkeys (WASD, Space...) while this is set.
    static bool IsTypingText();

    // Sliders work in the slider's own range (0..1 unless SetSliderRange changed it).
    static void SetSliderValue(GameObject* slider, float value);
    static float GetSliderValue(GameObject* slider);
    static float GetSliderNormalized(GameObject* slider);
    static void SetSliderRange(GameObject* slider, float minValue, float maxValue, bool wholeNumbers = false);

    // Progress bars work in the bar's own range (0..1 unless SetProgressRange changed it).
    static void SetProgress(GameObject* bar, float value);
    static float GetProgress(GameObject* bar);
    static float GetProgressNormalized(GameObject* bar);
    static void SetProgressRange(GameObject* bar, float minValue, float maxValue);
    static void SetProgressColor(GameObject* bar, Color fill);
    static void SetImageTexture(GameObject* image, const std::string& path);

    // ---- placing widgets from script (world-anchored UI such as enemy overhead bars)
    // A widget's Transform2D position (offset of its pivot from its anchor reference point) and size, in canvas units.
    static void SetPosition(GameObject* widget, Vec2 position);
    static Vec2 GetPosition(GameObject* widget);
    static void SetSize(GameObject* widget, Vec2 size);

    // Projects a world point through the active game camera. WorldToViewport gives 0..1 across the game view
    // (x right, y down); WorldToCanvas gives canvas units of the canvas `widget` sits under (origin top-left,
    // +y down - what a widget anchored at the canvas' top-left corner takes as its position). Both return false
    // when the point is behind the camera, past its far plane, or there is no active camera; `out` is still
    // written when the point is merely off-screen.
    static bool WorldToViewport(Vec3 world, Vec2& out);
    static bool WorldToCanvas(GameObject* widget, Vec3 world, Vec2& out);

    // ---- pointer over the game view (aiming)
    // Where the mouse is over the game image: 0..1 (x right, y down). False while it is outside the image (or
    // over another editor panel); `out` still holds the last position.
    static bool GetGamePointer(Vec2& out);
    // Mouse wheel notches this frame while the pointer is over the game image (+ = away from the user).
    static float GetMouseWheel();
    // The camera ray through a game-view point (0..1, as GetGamePointer / WorldToViewport): origin on the near
    // plane, unit direction. The inverse of WorldToViewport. False without an active camera.
    static bool ScreenToRay(Vec2 viewport, Vec3& origin, Vec3& direction);
};

} // namespace Phoenix
