#pragma once
#include "Module.h"
#include "UIDrawItem.h"
#include "UITypes.h"
#include "ComponentTransform2D.h"
#include "API/Phoenix_Keys.h"
#include <memory>
#include <string>
#include <vector>

class GameObject;
class RenderTexture;
class SceneGraph;
class UIPass;
class ComponentSelectable;
class ComponentInputBox;

// Owns the 2D UI: fonts, layout of every ComponentCanvas hierarchy, pointer/keyboard interaction, and drawing
// it over a finished frame.
//
// UI is drawn in canvas units (screen size / canvas scale factor) with the origin at the top-left and +Y down,
// after the 3D scene has been tonemapped, so it is never tone-mapped, bloomed or post-processed.
//
// This header deliberately avoids DirectXTK12 so GameScript.dll can include it for the inline getters.
class ModuleUI : public Module {
public:
    ModuleUI();
    ~ModuleUI() override;

    bool init() override;
    bool cleanUp() override;

    // Feed one frame of input: lay out the canvases, work out which widget is under the pointer and update button
    // states and one-shot flags, then dispatch the resulting events to listeners. Call once per frame, after
    // game scripts have updated and before the frame is rendered.
    void updateInteraction(SceneGraph* scene, uint32_t width, uint32_t height, const UIInput& input);

    // Lay out and draw all enabled canvases of `scene` into `target` (an R8G8B8A8_UNORM render texture that
    // is currently in the pixel-shader-resource state, as left by the tonemap pass).
    void renderUI(ID3D12GraphicsCommandList* cmd, RenderTexture* target, SceneGraph* scene);

    // Drops every script-registered listener. Must run before script code is unloaded.
    void clearAllListeners(SceneGraph* scene);

    // Characters typed into the window since the last call (printable ASCII). The standalone player feeds these
    // from WM_CHAR; the editor reads ImGui's queue instead.
    void pushTypedChar(uint32_t codepoint){ if (codepoint >= 32 && codepoint <= 126) m_typed += static_cast<char>(codepoint); }
    std::string takeTypedText(){ std::string s; s.swap(m_typed); return s; }

    // Editing-key events from the window (WM_KEYDOWN, so Windows' own key-repeat is included and no press is lost
    // between polled frames). The standalone player feeds these; the editor reads ImGui instead.
    struct EditKeys {
        int back = 0, del = 0, left = 0, right = 0;
        bool home = false, end = false, enter = false, escape = false, paste = false;
    };
    void pushEditKey(uint32_t virtualKey, bool ctrlDown){
        using Phoenix::Key;
        switch (static_cast<Key>(virtualKey)){
        case Key::Back:   ++m_keys.back; break;
        case Key::Delete: ++m_keys.del; break;
        case Key::Left:   ++m_keys.left; break;
        case Key::Right:  ++m_keys.right; break;
        case Key::Home:   m_keys.home = true; break;
        case Key::End:    m_keys.end = true; break;
        case Key::Enter:  m_keys.enter = true; break;
        case Key::Escape: m_keys.escape = true; break;
        case Key::V:      if (ctrlDown) m_keys.paste = true; break;
        default: break;
        }
    }
    EditKeys takeEditKeys(){ EditKeys k = m_keys; m_keys = EditKeys(); return k; }

    // True while an InputBox has focus: game code should ignore its hotkeys, and the editor its shortcuts.
    bool isTextInputActive() const { return m_textInputActive; }

    // True while the pointer is over an input-blocking widget, so game code can ignore clicks the UI owns.
    bool isPointerOverUI() const { return m_pointerOverUI; }

    UIPass* getPass() const { return m_pass.get(); }

    static constexpr const char* kDefaultFont = "UIFont";

private:
    // A widget that can receive the pointer, in draw order (later = on top).
    struct Hit {
        GameObject* go = nullptr;
        UIRect rect;              // canvas units
        Vector2 pivot;            // canvas units
        float rotation = 0.f;     // radians
        float scale = 1.f;        // canvas units -> pixels
    };

    struct PendingEvent {
        uint32_t uid;
        UIEventType type;
    };

    void buildDrawList(SceneGraph* scene, uint32_t width, uint32_t height);
    void collectCanvases(GameObject* node, std::vector<GameObject*>& out) const;
    void emitNode(GameObject* node, const UIRect& parentRect, float scale);
    void emitLabel(GameObject* node, const UIRect& rect, float scale);
    void emitProgressBar(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale);
    void emitCheckBox(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale);
    void emitSlider(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale);
    void pushSubRect(const Vector2& mn, const Vector2& mx, const Vector2& pivotPos, float rotation, float scale,
                     const std::string& texture, const Vector4& color, const Vector4* uv);
    Vector2 toLocal(const Hit& hit, const Vector2& pixel) const;
    void emitInputBox(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale);
    void pushText(const std::string& font, const std::string& text, const Vector4& color, const Vector2& topLeft,
                  float fontScale, const Vector2& pivotPos, float rotation, float scale);
    std::string resolveFont(const std::string& name) const;
    double nowMs() const;
    int caretIndexAt(const ComponentInputBox& box, const Hit& hit, const Vector2& pixel) const;
    bool hitTest(const Hit& hit, const Vector2& pixel) const;
    void dispatch(SceneGraph* scene);

    std::unique_ptr<UIPass> m_pass;
    std::vector<UIDrawItem> m_items;
    std::vector<Hit> m_hits;
    std::vector<PendingEvent> m_pending;
    bool m_pointerOverUI = false;
    bool m_textInputActive = false;
    std::string m_typed;
    EditKeys m_keys;
};
