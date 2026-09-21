#pragma once
#include "Component.h"
#include "UIDelegate.h"
#include "UIJson.h"
#include "UITypes.h"
#include <string>

// What every interactive widget (Button, CheckBox, Slider) shares: it can be hovered, pressed and focused with
// Tab, can be disabled, and raises the same pointer events. ModuleUI drives this state; game code reads it.
//
// Everything a script needs is plain data or inline code, so GameScript.dll can use it without the renderer.
class ComponentSelectable : public Component {
public:
    enum class State { Normal = 0, Hovered, Pressed, Disabled };

    explicit ComponentSelectable(GameObject* owner) : Component(owner){}

    // ---- authoring ----
    bool interactable = true;
    bool navigable = true;   // reachable with Tab

    // ---- runtime state (written by ModuleUI) ----
    State state = State::Normal;
    bool hovered = false;
    bool focused = false;
    bool mouseHeld = false;
    bool keyHeld = false;

    // One-shot flags: set during a UI update, valid until the next one (so a script's Update sees them).
    bool clicked = false;
    bool pressedThisFrame = false;
    bool releasedThisFrame = false;

    bool isHeld() const { return mouseHeld || keyHeld; }

    // ---- events ----
    UIDelegate onClick;
    UIDelegate onPress;
    UIDelegate onRelease;
    UIDelegate onHoverEnter;
    UIDelegate onHoverExit;

    UIDelegate& delegateFor(UIEventType type){
        switch (type){
        case UIEventType::HoverEnter: return onHoverEnter;
        case UIEventType::HoverExit:  return onHoverExit;
        case UIEventType::Press:      return onPress;
        case UIEventType::Release:    return onRelease;
        default:                      return onClick;
        }
    }

    virtual void clearListeners(){
        onClick.clear(); onPress.clear(); onRelease.clear(); onHoverEnter.clear(); onHoverExit.clear();
    }

    // Colour multiplier for widgets that don't have their own per-state colours.
    static Vector4 stateTint(State s){
        switch (s){
        case State::Hovered:  return Vector4(0.88f, 0.88f, 0.88f, 1.f);
        case State::Pressed:  return Vector4(0.7f, 0.7f, 0.7f, 1.f);
        case State::Disabled: return Vector4(0.5f, 0.5f, 0.5f, 0.6f);
        default:              return Vector4(1.f, 1.f, 1.f, 1.f);
        }
    }

protected:
    void saveSelectable(std::string& outJson) const {
        UIJson::putBool(outJson, "interactable", interactable);
        UIJson::putBool(outJson, "navigable", navigable);
    }

    void loadSelectable(const UIJson::Reader& r){
        r.getBool("interactable", interactable);
        r.getBool("navigable", navigable);
    }
};
