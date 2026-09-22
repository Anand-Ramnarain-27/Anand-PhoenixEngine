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
#include "ComponentCheckBox.h"
#include "ComponentSlider.h"
#include "ComponentInputBox.h"
#include "UISelectable.h"
#include "UIPass.h"
#include <algorithm>
#include <chrono>
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
        if (auto* widget = selectableOf(node)) widget->clearListeners();
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
    if (selectableOf(node) || (drawsImage && image->raycastTarget))
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

    if (node->getComponent<ComponentCheckBox>()) emitCheckBox(node, rect, pivotPos, rotation, scale);
    if (node->getComponent<ComponentSlider>()) emitSlider(node, rect, pivotPos, rotation, scale);
    if (node->getComponent<ComponentInputBox>()) emitInputBox(node, rect, pivotPos, rotation, scale);

    if (auto* label = node->getComponent<ComponentLabel>(); label && label->enabled && !label->text.empty())
        emitLabel(node, rect, scale);

    for (GameObject* child : node->getChildren()) emitNode(child, rect, scale);
}

// Draws a sub-rectangle of a widget. It is positioned by rotating its corner about the widget pivot, so a rotated
// widget still rotates as one piece.
void ModuleUI::pushSubRect(const Vector2& mn, const Vector2& mx, const Vector2& pivotPos, float rotation, float scale,
                           const std::string& texture, const Vector4& color, const Vector4* uv){
    const float c = std::cos(rotation), s = std::sin(rotation);
    const Vector2 off = mn - pivotPos;

    UIDrawItem item;
    item.kind = UIDrawItem::Kind::Image;
    item.texture = texture;
    item.position = (pivotPos + Vector2(off.x * c - off.y * s, off.x * s + off.y * c)) * scale;
    item.size = (mx - mn) * scale;
    item.pivot = Vector2::Zero;
    item.rotation = rotation;
    item.color = color;
    // A flat colour has nothing to crop; a texture is cropped so it is not squashed into a partial rect.
    if (uv && !texture.empty()){
        item.useSourceUV = true;
        item.sourceUV = *uv;
    }
    m_items.push_back(std::move(item));
}

void ModuleUI::emitProgressBar(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale){
    auto* bar = node->getComponent<ComponentProgressBar>();

    pushSubRect(rect.min, rect.max, pivotPos, rotation, scale, bar->backgroundTexture, bar->backgroundColor, nullptr);

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
    pushSubRect(mn, mx, pivotPos, rotation, scale, bar->fillTexture, bar->fillColor, &uv);
}

void ModuleUI::emitCheckBox(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale){
    auto* box = node->getComponent<ComponentCheckBox>();
    const Vector4 tint = ComponentSelectable::stateTint(box->state);

    // The box hugs the left edge of the row, vertically centred.
    const float side = box->boxSize > 0.f ? box->boxSize : rect.size().y;
    const Vector2 boxMin(rect.min.x, rect.center().y - side * 0.5f);
    const Vector2 boxMax = boxMin + Vector2(side, side);
    pushSubRect(boxMin, boxMax, pivotPos, rotation, scale, std::string(), box->boxColor * tint, nullptr);

    if (!box->checked) return;
    const float inset = side * 0.22f;
    pushSubRect(boxMin + Vector2(inset, inset), boxMax - Vector2(inset, inset), pivotPos, rotation, scale,
                box->checkTexture, box->checkColor * tint, nullptr);
}

void ModuleUI::emitSlider(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale){
    auto* slider = node->getComponent<ComponentSlider>();
    const Vector4 tint = ComponentSelectable::stateTint(slider->state);
    const bool horizontal = slider->isHorizontal();
    const Vector2 mid = rect.center();

    // Handle centre along the main axis, and the thin track through the middle of the rect.
    const float pos = slider->positionAt(rect, slider->getNormalized());
    const float half = slider->trackThickness * 0.5f;
    UIRect track;
    track.min = horizontal ? Vector2(rect.min.x, mid.y - half) : Vector2(mid.x - half, rect.min.y);
    track.max = horizontal ? Vector2(rect.max.x, mid.y + half) : Vector2(mid.x + half, rect.max.y);
    pushSubRect(track.min, track.max, pivotPos, rotation, scale, std::string(), slider->trackColor, nullptr);

    // Fill runs from the track's start (the end the value grows from) to the handle.
    UIRect fill = track;
    switch (slider->direction){
    case ComponentSlider::Direction::LeftToRight: fill.max.x = pos; break;
    case ComponentSlider::Direction::RightToLeft: fill.min.x = pos; break;
    case ComponentSlider::Direction::BottomToTop: fill.min.y = pos; break;
    case ComponentSlider::Direction::TopToBottom: fill.max.y = pos; break;
    }
    const Vector2 fs = fill.size();
    if (fs.x > 0.f && fs.y > 0.f)
        pushSubRect(fill.min, fill.max, pivotPos, rotation, scale, std::string(), slider->fillColor, nullptr);

    const Vector2 hs = slider->handleSize;
    const Vector2 hMin = horizontal ? Vector2(pos - hs.x * 0.5f, mid.y - hs.y * 0.5f) : Vector2(mid.x - hs.x * 0.5f, pos - hs.y * 0.5f);
    pushSubRect(hMin, hMin + hs, pivotPos, rotation, scale, std::string(), slider->handleColor * tint, nullptr);
}

void ModuleUI::pushText(const std::string& font, const std::string& text, const Vector4& color, const Vector2& topLeft,
                        float fontScale, const Vector2& pivotPos, float rotation, float scale){
    UIDrawItem item;
    item.kind = UIDrawItem::Kind::Text;
    item.text = text;
    item.font = font;
    item.color = color;
    item.position = pivotPos * scale;
    item.origin = (pivotPos - topLeft) / fontScale;   // rotate about the widget pivot
    item.scale = fontScale * scale;
    item.rotation = rotation;
    m_items.push_back(std::move(item));
}

std::string ModuleUI::resolveFont(const std::string& name) const{
    return m_pass->hasFont(name) ? name : std::string(kDefaultFont);
}

double ModuleUI::nowMs() const{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

int ModuleUI::caretIndexAt(const ComponentInputBox& box, const Hit& hit, const Vector2& pixel) const{
    const std::string font = resolveFont(box.fontName);
    const float lineSpacing = m_pass->getLineSpacing(font);
    if (lineSpacing <= 0.f) return (int)box.text.size();
    const float fontScale = box.fontSize / lineSpacing;

    // Pointer x measured from where the text starts (accounting for scroll), then the nearest character boundary.
    const float x = toLocal(hit, pixel).x - (hit.rect.min.x + box.padding.x - box.scrollX);
    const std::string display = box.displayText();
    int best = 0;
    float bestDistance = std::abs(x);
    for (int i = 1; i <= (int)display.size(); ++i){
        const float w = m_pass->measureText(font, display.substr(0, i), false).x * fontScale;
        const float d = std::abs(x - w);
        if (d < bestDistance){ best = i; bestDistance = d; }
    }
    return best;
}

void ModuleUI::emitInputBox(GameObject* node, const UIRect& rect, const Vector2& pivotPos, float rotation, float scale){
    auto* box = node->getComponent<ComponentInputBox>();
    const Vector4 tint = ComponentSelectable::stateTint(box->state);

    if (box->focused)
        pushSubRect(rect.min - Vector2(2.f, 2.f), rect.max + Vector2(2.f, 2.f), pivotPos, rotation, scale, std::string(), box->focusColor, nullptr);
    pushSubRect(rect.min, rect.max, pivotPos, rotation, scale, std::string(), box->backgroundColor * tint, nullptr);

    const std::string font = resolveFont(box->fontName);
    const float lineSpacing = m_pass->getLineSpacing(font);
    UIRect inner;
    inner.min = rect.min + box->padding;
    inner.max = rect.max - box->padding;
    const Vector2 innerSize = inner.size();
    if (lineSpacing <= 0.f || innerSize.x <= 0.f || innerSize.y <= 0.f) return;

    const float fontScale = box->fontSize / lineSpacing;
    const std::string display = box->displayText();
    const float textWidth = m_pass->measureText(font, display, false).x * fontScale;
    const float caretX = m_pass->measureText(font, display.substr(0, box->caret), false).x * fontScale;

    // Scroll just enough to keep the caret in view; text that fits does not scroll.
    const float viewWidth = innerSize.x - 2.f;
    if (textWidth <= viewWidth){
        box->scrollX = 0.f;
    }
    else {
        if (caretX - box->scrollX > viewWidth) box->scrollX = caretX - viewWidth;
        if (caretX - box->scrollX < 0.f) box->scrollX = caretX;
        box->scrollX = std::clamp(box->scrollX, 0.f, textWidth - viewWidth);
    }

    const size_t firstClipped = m_items.size();
    const float top = inner.center().y - box->fontSize * 0.5f;
    if (!display.empty())
        pushText(font, display, box->textColor, Vector2(inner.min.x - box->scrollX, top), fontScale, pivotPos, rotation, scale);
    else if (!box->placeholder.empty())
        pushText(font, box->placeholder, box->placeholderColor, Vector2(inner.min.x, top), fontScale, pivotPos, rotation, scale);

    // The caret blinks while focused, and stays solid for a moment after every edit.
    if (box->focused && box->interactable && std::fmod(nowMs() - box->blinkStart, 1060.0) < 530.0){
        const Vector2 caretMin(inner.min.x - box->scrollX + caretX, top + box->fontSize * 0.05f);
        pushSubRect(caretMin, caretMin + Vector2(2.f, box->fontSize * 0.9f), pivotPos, rotation, scale, std::string(), box->caretColor, nullptr);
    }

    // Text and caret are clipped to the field. (The clip is axis-aligned, so rotated fields draw unclipped.)
    if (rotation == 0.f){
        for (size_t i = firstClipped; i < m_items.size(); ++i){
            m_items[i].clip = true;
            m_items[i].clipRect = Vector4(inner.min.x * scale, inner.min.y * scale, innerSize.x * scale, innerSize.y * scale);
        }
    }
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

    pushText(fontName, label->text, label->color, topLeft, fontScale, pivotPos, rotation, scale);
}

Vector2 ModuleUI::toLocal(const Hit& hit, const Vector2& pixel) const{
    Vector2 p = pixel / hit.scale;
    if (hit.rotation != 0.f){
        // Undo the widget's rotation about its pivot so tests run against the unrotated rect.
        const Vector2 d = p - hit.pivot;
        const float c = std::cos(-hit.rotation), s = std::sin(-hit.rotation);
        p = hit.pivot + Vector2(d.x * c - d.y * s, d.x * s + d.y * c);
    }
    return p;
}

bool ModuleUI::hitTest(const Hit& hit, const Vector2& pixel) const{
    return hit.rect.contains(toLocal(hit, pixel));
}

void ModuleUI::updateInteraction(SceneGraph* scene, uint32_t width, uint32_t height, const UIInput& in){
    m_pending.clear();
    m_pointerOverUI = false;
    if (!scene || width == 0 || height == 0) return;

    buildDrawList(scene, width, height);

    struct Widget {
        GameObject* go;
        ComponentSelectable* sel;
        const Hit* hit;
    };
    std::vector<Widget> widgets;
    for (const Hit& hit : m_hits){
        ComponentSelectable* sel = selectableOf(hit.go);
        if (!sel) continue;
        sel->clicked = sel->pressedThisFrame = sel->releasedThisFrame = false;
        widgets.push_back({ hit.go, sel, &hit });
    }

    // The topmost input-blocking element under the pointer wins; the widget that handles it is the nearest one
    // at or above it, so a label or icon inside a button or checkbox row still counts as that widget.
    ComponentSelectable* hoverWidget = nullptr;
    if (in.pointerValid){
        for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it){
            if (!hitTest(*it, in.pointer)) continue;
            m_pointerOverUI = true;
            for (GameObject* g = it->go; g && !hoverWidget; g = g->getParent())
                for (const Widget& w : widgets)
                    if (w.go == g){ hoverWidget = w.sel; break; }
            break;
        }
    }

    auto queue = [&](const Widget& w, UIEventType type){ m_pending.push_back({ w.go->getUID(), type }); };

    // A click toggles a checkbox; the change is queued like any other event.
    auto onClicked = [&](const Widget& w){
        w.sel->clicked = true;
        queue(w, UIEventType::Click);
        if (auto* box = w.go->getComponent<ComponentCheckBox>()){
            box->checked = !box->checked;
            queue(w, UIEventType::ValueChanged);
        }
    };

    // Hover enter/exit. A disabled widget blocks the pointer but is never hovered.
    for (const Widget& w : widgets){
        const bool nowHover = w.sel == hoverWidget && w.sel->interactable;
        if (nowHover && !w.sel->hovered) queue(w, UIEventType::HoverEnter);
        if (!nowHover && w.sel->hovered) queue(w, UIEventType::HoverExit);
        w.sel->hovered = nowHover;
    }

    // Pointer press / release. A click is a release over the same widget that was pressed.
    if (in.mousePressed){
        for (const Widget& w : widgets) w.sel->focused = false;
        if (hoverWidget && hoverWidget->interactable){
            for (const Widget& w : widgets){
                if (w.sel != hoverWidget) continue;
                w.sel->mouseHeld = true;
                w.sel->focused = w.sel->navigable;
                w.sel->pressedThisFrame = true;
                queue(w, UIEventType::Press);

                if (auto* box = w.go->getComponent<ComponentInputBox>()){
                    w.sel->focused = true;   // clicking a text field always starts editing
                    box->setCaret(caretIndexAt(*box, *w.hit, in.pointer));
                    box->blinkStart = nowMs();
                }
            }
        }
    }
    if (in.mouseReleased){
        for (const Widget& w : widgets){
            if (!w.sel->mouseHeld) continue;
            w.sel->mouseHeld = false;
            w.sel->releasedThisFrame = true;
            queue(w, UIEventType::Release);
            if (w.sel->hovered) onClicked(w);
        }
    }

    // Keyboard: Tab / Shift+Tab walks the navigable widgets in draw order, Enter/Space presses the focused one.
    std::vector<const Widget*> navigable;
    for (const Widget& w : widgets){
        if (w.sel->interactable && w.sel->navigable) navigable.push_back(&w);
        const bool keepsFocus = w.sel->navigable || w.go->getComponent<ComponentInputBox>();
        if (!w.sel->interactable || !keepsFocus) w.sel->focused = false;
    }
    if (in.tabPressed && !navigable.empty()){
        const int count = (int)navigable.size();
        int current = -1;
        for (int i = 0; i < count; ++i) if (navigable[i]->sel->focused) current = i;
        const int step = in.shiftDown ? -1 : 1;
        const int next = current < 0 ? (in.shiftDown ? count - 1 : 0) : (current + step + count) % count;
        for (const Widget* w : navigable) w->sel->focused = false;
        navigable[next]->sel->focused = true;
    }
    if (in.submitPressed){
        for (const Widget* w : navigable){
            if (!w->sel->focused) continue;
            if (w->go->getComponent<ComponentInputBox>()) continue;   // Space types a space, Enter submits
            w->sel->keyHeld = true;
            w->sel->pressedThisFrame = true;
            queue(*w, UIEventType::Press);
        }
    }
    if (in.submitReleased){
        for (const Widget& w : widgets){
            if (!w.sel->keyHeld) continue;
            w.sel->keyHeld = false;
            w.sel->releasedThisFrame = true;
            queue(w, UIEventType::Release);
            onClicked(w);
        }
    }

    // Text: the focused InputBox takes typed characters, paste, and the editing keys.
    m_textInputActive = false;
    for (const Widget& w : widgets){
        auto* box = w.go->getComponent<ComponentInputBox>();
        if (!box || !w.sel->focused || !w.sel->interactable) continue;
        m_textInputActive = true;

        bool changed = box->insertText(in.text);
        changed |= box->insertText(in.paste);
        for (int i = 0; i < in.backspace; ++i) changed |= box->eraseBefore();
        for (int i = 0; i < in.deleteKey; ++i) changed |= box->eraseAfter();
        if (in.navX != 0) box->moveCaret(in.navX);
        if (in.home) box->setCaret(0);
        if (in.end) box->setCaret((int)box->text.size());
        if (changed || in.navX != 0 || in.home || in.end) box->blinkStart = nowMs();   // caret stays solid while editing

        if (changed) queue(w, UIEventType::ValueChanged);
        if (in.enterPressed) queue(w, UIEventType::Submit);
        if (in.escapePressed) w.sel->focused = false;
    }

    // Sliders: follow the pointer while it is held on one (clamped, so dragging past the ends is fine), and let
    // the arrow keys nudge the focused one.
    for (const Widget& w : widgets){
        auto* slider = w.go->getComponent<ComponentSlider>();
        if (!slider || !slider->interactable) continue;

        bool changed = false;
        if (w.sel->mouseHeld && in.pointerValid)
            changed = slider->setNormalized(slider->normalizedAt(w.hit->rect, toLocal(*w.hit, in.pointer)));

        if (w.sel->focused){
            // Left/Right for horizontal sliders, Up/Down for vertical; the sign follows the fill direction.
            const int nav = slider->isHorizontal() ? in.navX : in.navY;
            const bool reversed = slider->direction == ComponentSlider::Direction::RightToLeft ||
                                  slider->direction == ComponentSlider::Direction::TopToBottom;
            if (nav != 0) changed |= slider->setValue(slider->value + slider->keyStep() * float(reversed ? -nav : nav));
        }
        if (changed) queue(w, UIEventType::ValueChanged);
    }

    for (const Widget& w : widgets){
        ComponentSelectable* sel = w.sel;
        if (!sel->interactable) sel->state = ComponentSelectable::State::Disabled;
        else if ((sel->mouseHeld && sel->hovered) || sel->keyHeld || (sel->mouseHeld && w.go->getComponent<ComponentSlider>()))
            sel->state = ComponentSelectable::State::Pressed;
        else if (sel->hovered || sel->focused) sel->state = ComponentSelectable::State::Hovered;
        else sel->state = ComponentSelectable::State::Normal;
    }

    dispatch(scene);
}

// Listeners run after all state is settled, and each target is looked up again by UID: a listener may destroy
// objects (including the widget it belongs to), which would leave any pointer gathered earlier dangling.
void ModuleUI::dispatch(SceneGraph* scene){
    std::vector<PendingEvent> events;
    events.swap(m_pending);

    for (const PendingEvent& e : events){
        GameObject* go = findByUid(scene->getRoot(), e.uid);
        ComponentSelectable* sel = selectableOf(go);
        if (!sel) continue;

        if (e.type == UIEventType::Submit){
            if (auto* input = go->getComponent<ComponentInputBox>()) input->onSubmit.invoke(input->text);
        }
        else if (e.type != UIEventType::ValueChanged){
            sel->delegateFor(e.type).invoke();
        }
        else if (auto* input = go->getComponent<ComponentInputBox>()){
            input->onValueChanged.invoke(input->text);
        }
        else if (auto* box = go->getComponent<ComponentCheckBox>()){
            box->onValueChanged.invoke(box->checked);
        }
        else if (auto* slider = go->getComponent<ComponentSlider>()){
            slider->onValueChanged.invoke(slider->value);
        }
    }
}

void ModuleUI::clearAllListeners(SceneGraph* scene){
    if (scene) clearListenersRecursive(scene->getRoot());
}
