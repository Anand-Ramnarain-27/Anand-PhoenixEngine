#pragma once
#include "Component.h"
#include "UITypes.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A list of callbacks. Invocation runs a snapshot, so a listener may add or remove listeners, or even
// destroy the button that owns this delegate, while it is being called.
class UIDelegate {
public:
    using Callback = std::function<void()>;

    uint32_t add(Callback cb){
        m_entries.push_back({ m_nextId, std::move(cb) });
        return m_nextId++;
    }

    bool remove(uint32_t id){
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it){
            if (it->id != id) continue;
            m_entries.erase(it);
            return true;
        }
        return false;
    }

    void clear(){ m_entries.clear(); }
    bool empty() const { return m_entries.empty(); }

    void invoke() const {
        const std::vector<Entry> snapshot = m_entries;
        for (const Entry& e : snapshot)
            if (e.fn) e.fn();
    }

private:
    struct Entry {
        uint32_t id;
        Callback fn;
    };

    std::vector<Entry> m_entries;
    uint32_t m_nextId = 1;
};

// A clickable widget. It reacts to the pointer over its ComponentTransform2D rect (or over a child Image that
// blocks input), tints/swaps the sibling ComponentImage by state, and raises events to game code.
//
// Everything a script needs is plain data or inline code here, so GameScript.dll can use it without linking
// the renderer.
class ComponentButton : public Component {
public:
    enum class State { Normal = 0, Hovered, Pressed, Disabled };

    explicit ComponentButton(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Button; }

    // ---- authoring ----
    bool interactable = true;
    bool navigable = true;   // reachable with Tab

    // Multiplied into the sibling Image's tint per state.
    Vector4 normalColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 hoverColor = Vector4(0.88f, 0.88f, 0.88f, 1.f);
    Vector4 pressedColor = Vector4(0.7f, 0.7f, 0.7f, 1.f);
    Vector4 disabledColor = Vector4(0.5f, 0.5f, 0.5f, 0.6f);

    // Optional sprite swap; an empty path keeps the Image's own texture.
    std::string hoverTexture;
    std::string pressedTexture;
    std::string disabledTexture;

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

    void clearListeners(){
        onClick.clear(); onPress.clear(); onRelease.clear(); onHoverEnter.clear(); onHoverExit.clear();
    }

    const Vector4& currentColor() const {
        switch (state){
        case State::Hovered:  return hoverColor;
        case State::Pressed:  return pressedColor;
        case State::Disabled: return disabledColor;
        default:              return normalColor;
        }
    }

    // The texture to draw for the current state, falling back to `base`.
    const std::string& currentTexture(const std::string& base) const {
        const std::string* swap = nullptr;
        switch (state){
        case State::Hovered:  swap = &hoverTexture; break;
        case State::Pressed:  swap = &pressedTexture; break;
        case State::Disabled: swap = &disabledTexture; break;
        default: break;
        }
        return (swap && !swap->empty()) ? *swap : base;
    }
};
