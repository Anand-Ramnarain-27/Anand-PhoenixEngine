#include "Globals.h"
#include "ModuleUI.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentCanvas.h"
#include "ComponentTransform2D.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentButton.h"
#include "ComponentProgressBar.h"
#include "UIPass.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
    GameObject* findByUid(GameObject* node, uint32_t uid){
        if (!node) return nullptr;
        if (node->getUID() == uid) return node;
        for (GameObject* child : node->getChildren())
            if (GameObject* found = findByUid(child, uid)) return found;
        return nullptr;
    }

    void clearListenersRecursive(GameObject* node){
        if (!node) return;
        if (auto* button = node->getComponent<ComponentButton>()) button->clearListeners();
        for (GameObject* child : node->getChildren()) clearListenersRecursive(child);
    }
}

ModuleUI::ModuleUI() = default;
ModuleUI::~ModuleUI() = default;

bool ModuleUI::init(){
    auto* d3d12 = app->getD3D12();
    m_pass = std::make_unique<UIPass>();
    if (!m_pass->init(d3d12->getDevice(), d3d12->getDrawCommandQueue())){
        m_pass.reset();
        return false;
    }

    // Font baked by tools/MakeSpriteFont.py and copied next to the executable.
    m_pass->loadFont(kDefaultFont, L"UIFont.spritefont");
    return true;
}

bool ModuleUI::cleanUp(){
    if (m_pass){
        app->getD3D12()->flush();
        m_pass->shutdown();
        m_pass.reset();
    }
    return true;
}

void ModuleUI::renderUI(ID3D12GraphicsCommandList* cmd, RenderTexture* target, SceneGraph* scene){
    if (!m_pass || !scene || !target || !target->isValid()) return;

    buildDrawList(scene, target->getWidth(), target->getHeight());
    if (m_items.empty()) return;

    // Earlier passes may have left other heaps bound; SpriteBatch needs the engine's SRV heap.
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    target->beginRender(cmd, /*clear=*/false);
    m_pass->render(cmd, m_items, target->getWidth(), target->getHeight());
    target->endRender(cmd);
}

void ModuleUI::collectCanvases(GameObject* node, std::vector<GameObject*>& out) const{
    if (!node || !node->isActive()) return;

    if (auto* canvas = node->getComponent<ComponentCanvas>()){
        // A canvas owns its whole subtree; nested canvases are just containers.
        if (canvas->enabled) out.push_back(node);
        return;
    }
    for (GameObject* child : node->getChildren()) collectCanvases(child, out);
}

void ModuleUI::buildDrawList(SceneGraph* scene, uint32_t width, uint32_t height){
    m_items.clear();
    m_hits.clear();

    std::vector<GameObject*> canvases;
    collectCanvases(scene->getRoot(), canvases);
    std::stable_sort(canvases.begin(), canvases.end(), [](GameObject* a, GameObject* b){
        return a->getComponent<ComponentCanvas>()->sortOrder < b->getComponent<ComponentCanvas>()->sortOrder;
    });

    for (GameObject* go : canvases){
        const float scale = go->getComponent<ComponentCanvas>()->getScaleFactor(float(width), float(height));
        UIRect root;
        root.max = Vector2(float(width), float(height)) / scale;
        for (GameObject* child : go->getChildren()) emitNode(child, root, scale);
    }
}

void ModuleUI::emitNode(GameObject* node, const UIRect& parentRect, float scale){
    if (!node || !node->isActive()) return;

    UIRect rect = parentRect;
    Vector2 pivotPos = parentRect.center();
    Vector2 pivot(0.5f, 0.5f);
    float rotation = 0.f;

    if (auto* t = node->getComponent<ComponentTransform2D>()){
        if (!t->visible) return;
        t->computeLayout(parentRect);
        rect = t->getRect();
        pivotPos = t->getPivotPosition();
        pivot = t->pivot;
        rotation = DirectX::XMConvertToRadians(t->rotation);
    }
    else {
        pivotPos = rect.center();
    }

    auto* button = node->getComponent<ComponentButton>();
    auto* image = node->getComponent<ComponentImage>();
    const bool drawsImage = image && image->enabled;

    // Anything that can take the pointer is recorded in draw order; hit-testing walks it back to front.
    if (button || (drawsImage && image->raycastTarget))
        m_hits.push_back({ node, rect, pivotPos, rotation, scale });

    if (drawsImage){
        UIDrawItem item;
        item.kind = UIDrawItem::Kind::Image;
        item.texture = button ? button->currentTexture(image->texturePath) : image->texturePath;
        item.position = pivotPos * scale;
        item.size = rect.size() * scale;
        item.pivot = pivot;
        item.rotation = rotation;
        item.color = button ? image->tint * button->currentColor() : image->tint;
        item.useSourceRect = image->useSourceRect;
        item.sourceRect = image->sourceRect;
        m_items.push_back(std::move(item));
    }

    if (auto* bar = node->getComponent<ComponentProgressBar>(); bar && bar->enabled)
        emitProgressBar(node, rect, pivotPos, rotation, scale);

    if (auto* label = node->getComponent<ComponentLabel>(); label && label->enabled && !label->text.empty())
        emitLabel(node, rect, scale);

    for (GameObject* child : node->getChildren()) emitNode(child, rect, scale);
}

void ModuleUI::emitProgressBar(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale){
    auto* bar = node->getComponent<ComponentProgressBar>();
    const float c = std::cos(rotation), s = std::sin(rotation);

    // Draws a sub-rectangle of the widget. It is positioned by rotating its corner about the widget pivot, so a
    // rotated bar still rotates as one piece.
    auto push = [&](Vector2 mn, Vector2 mx, const std::string& texture, const Vector4& color, const Vector4* uv){
        const Vector2 off = mn - pivotPos;
        UIDrawItem item;
        item.kind = UIDrawItem::Kind::Image;
        item.texture = texture;
        item.position = (pivotPos + Vector2(off.x * c - off.y * s, off.x * s + off.y * c)) * scale;
        item.size = (mx - mn) * scale;
        item.pivot = Vector2::Zero;
        item.rotation = rotation;
        item.color = color;
        // A flat colour has nothing to crop; a texture is cropped so it is not squashed into the filled part.
        if (uv && !texture.empty()){
            item.useSourceUV = true;
            item.sourceUV = *uv;
        }
        m_items.push_back(std::move(item));
    };

    push(rect.min, rect.max, bar->backgroundTexture, bar->backgroundColor, nullptr);

    const float f = bar->getNormalized();
    if (f <= 0.f) return;

    Vector2 mn = rect.min, mx = rect.max;
    const Vector2 size = rect.size();
    Vector4 uv(0.f, 0.f, 1.f, 1.f);
    switch (bar->direction){
    case ComponentProgressBar::FillDirection::LeftToRight: mx.x = mn.x + size.x * f; uv = Vector4(0.f, 0.f, f, 1.f); break;
    case ComponentProgressBar::FillDirection::RightToLeft: mn.x = mx.x - size.x * f; uv = Vector4(1.f - f, 0.f, f, 1.f); break;
    case ComponentProgressBar::FillDirection::BottomToTop: mn.y = mx.y - size.y * f; uv = Vector4(0.f, 1.f - f, 1.f, f); break;
    case ComponentProgressBar::FillDirection::TopToBottom: mx.y = mn.y + size.y * f; uv = Vector4(0.f, 0.f, 1.f, f); break;
    }
    push(mn, mx, bar->fillTexture, bar->fillColor, &uv);
}

void ModuleUI::emitLabel(GameObject* node, const UIRect& rect, float scale){
    auto* label = node->getComponent<ComponentLabel>();
    const std::string& fontName = m_pass->hasFont(label->fontName) ? label->fontName : std::string(kDefaultFont);

    const float lineSpacing = m_pass->getLineSpacing(fontName);
    if (lineSpacing <= 0.f) return;

    // fontSize is the line height in canvas units; convert to a scale over the baked font pixels.
    const float fontScale = label->fontSize / lineSpacing;
    const Vector2 textSize = m_pass->measureText(fontName, label->text) * fontScale;

    Vector2 topLeft = rect.min;
    switch (label->hAlign){
    case ComponentLabel::HAlign::Left:   break;
    case ComponentLabel::HAlign::Center: topLeft.x = rect.center().x - textSize.x * 0.5f; break;
    case ComponentLabel::HAlign::Right:  topLeft.x = rect.max.x - textSize.x; break;
    }
    switch (label->vAlign){
    case ComponentLabel::VAlign::Top:    break;
    case ComponentLabel::VAlign::Middle: topLeft.y = rect.center().y - textSize.y * 0.5f; break;
    case ComponentLabel::VAlign::Bottom: topLeft.y = rect.max.y - textSize.y; break;
    }

    // Rotate about the widget pivot: draw from the pivot, with the text origin offset to match.
    Vector2 pivotPos = rect.center();
    float rotation = 0.f;
    if (auto* t = node->getComponent<ComponentTransform2D>()){
        pivotPos = t->getPivotPosition();
        rotation = DirectX::XMConvertToRadians(t->rotation);
    }

    UIDrawItem item;
    item.kind = UIDrawItem::Kind::Text;
    item.text = label->text;
    item.font = fontName;
    item.color = label->color;
    item.position = pivotPos * scale;
    item.origin = (pivotPos - topLeft) / fontScale;
    item.scale = fontScale * scale;
    item.rotation = rotation;
    m_items.push_back(std::move(item));
}

bool ModuleUI::hitTest(const Hit& hit, const Vector2& pixel) const{
    Vector2 p = pixel / hit.scale;
    if (hit.rotation != 0.f){
        // Undo the widget's rotation about its pivot so the test runs against the unrotated rect.
        const Vector2 d = p - hit.pivot;
        const float c = std::cos(-hit.rotation), s = std::sin(-hit.rotation);
        p = hit.pivot + Vector2(d.x * c - d.y * s, d.x * s + d.y * c);
    }
    return hit.rect.contains(p);
}

void ModuleUI::updateInteraction(SceneGraph* scene, uint32_t width, uint32_t height, const UIInput& in){
    m_pending.clear();
    m_pointerOverUI = false;
    if (!scene || width == 0 || height == 0) return;

    buildDrawList(scene, width, height);

    struct ButtonRef {
        GameObject* go;
        ComponentButton* button;
    };
    std::vector<ButtonRef> buttons;
    for (const Hit& hit : m_hits){
        auto* button = hit.go->getComponent<ComponentButton>();
        if (!button) continue;
        button->clicked = button->pressedThisFrame = button->releasedThisFrame = false;
        buttons.push_back({ hit.go, button });
    }

    // The topmost input-blocking widget under the pointer wins; the button that handles it is the nearest
    // one at or above it, so an icon or label inside a button still counts as the button.
    ComponentButton* hoverButton = nullptr;
    if (in.pointerValid){
        for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it){
            if (!hitTest(*it, in.pointer)) continue;
            m_pointerOverUI = true;
            for (GameObject* g = it->go; g && !hoverButton; g = g->getParent())
                for (const ButtonRef& r : buttons)
                    if (r.go == g){ hoverButton = r.button; break; }
            break;
        }
    }

    auto queue = [&](const ButtonRef& r, UIEventType type){ m_pending.push_back({ r.go->getUID(), type }); };

    // Hover enter/exit. A disabled button blocks the pointer but is never hovered.
    for (const ButtonRef& r : buttons){
        const bool nowHover = r.button == hoverButton && r.button->interactable;
        if (nowHover && !r.button->hovered) queue(r, UIEventType::HoverEnter);
        if (!nowHover && r.button->hovered) queue(r, UIEventType::HoverExit);
        r.button->hovered = nowHover;
    }

    // Pointer press / release. A click is a release over the same button that was pressed.
    if (in.mousePressed){
        for (const ButtonRef& r : buttons) r.button->focused = false;
        if (hoverButton && hoverButton->interactable){
            for (const ButtonRef& r : buttons){
                if (r.button != hoverButton) continue;
                r.button->mouseHeld = true;
                r.button->focused = r.button->navigable;
                r.button->pressedThisFrame = true;
                queue(r, UIEventType::Press);
            }
        }
    }
    if (in.mouseReleased){
        for (const ButtonRef& r : buttons){
            if (!r.button->mouseHeld) continue;
            r.button->mouseHeld = false;
            r.button->releasedThisFrame = true;
            queue(r, UIEventType::Release);
            if (r.button->hovered){
                r.button->clicked = true;
                queue(r, UIEventType::Click);
            }
        }
    }

    // Keyboard: Tab / Shift+Tab walks the navigable buttons in draw order, Enter/Space presses the focused one.
    std::vector<const ButtonRef*> navigable;
    for (const ButtonRef& r : buttons){
        if (r.button->interactable && r.button->navigable) navigable.push_back(&r);
        else r.button->focused = false;
    }
    if (in.tabPressed && !navigable.empty()){
        const int count = (int)navigable.size();
        int current = -1;
        for (int i = 0; i < count; ++i) if (navigable[i]->button->focused) current = i;
        const int step = in.shiftDown ? -1 : 1;
        const int next = current < 0 ? (in.shiftDown ? count - 1 : 0) : (current + step + count) % count;
        for (const ButtonRef* r : navigable) r->button->focused = false;
        navigable[next]->button->focused = true;
    }
    if (in.submitPressed){
        for (const ButtonRef* r : navigable){
            if (!r->button->focused) continue;
            r->button->keyHeld = true;
            r->button->pressedThisFrame = true;
            queue(*r, UIEventType::Press);
        }
    }
    if (in.submitReleased){
        for (const ButtonRef& r : buttons){
            if (!r.button->keyHeld) continue;
            r.button->keyHeld = false;
            r.button->releasedThisFrame = true;
            queue(r, UIEventType::Release);
            r.button->clicked = true;
            queue(r, UIEventType::Click);
        }
    }

    for (const ButtonRef& r : buttons){
        ComponentButton* btn = r.button;
        if (!btn->interactable) btn->state = ComponentButton::State::Disabled;
        else if ((btn->mouseHeld && btn->hovered) || btn->keyHeld) btn->state = ComponentButton::State::Pressed;
        else if (btn->hovered || btn->focused) btn->state = ComponentButton::State::Hovered;
        else btn->state = ComponentButton::State::Normal;
    }

    dispatch(scene);
}

// Listeners run after all state is settled, and each target is looked up again by UID: a listener may destroy
// objects (including the button it belongs to), which would leave any pointer gathered earlier dangling.
void ModuleUI::dispatch(SceneGraph* scene){
    std::vector<PendingEvent> events;
    events.swap(m_pending);

    for (const PendingEvent& e : events){
        GameObject* go = findByUid(scene->getRoot(), e.uid);
        if (!go) continue;
        if (auto* button = go->getComponent<ComponentButton>())
            button->delegateFor(e.type).invoke();
    }
}

void ModuleUI::clearAllListeners(SceneGraph* scene){
    if (scene) clearListenersRecursive(scene->getRoot());
}
