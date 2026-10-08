#include "Globals.h"
#include "API/Phoenix_UI.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ModuleUI.h"
#include "UISelectable.h"
#include "ComponentProgressBar.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentTransform2D.h"
#include "UIRadioGroup.h"
#include "ComponentCanvas.h"
#include "ComponentCamera.h"
#include "ComponentTransform.h"
#include "ModuleCamera.h"
#include <algorithm>
#include <cmath>

// Everything here works on component data and inline code only: GameScript.dll shares just the `app` pointer
// with the engine, so it cannot call into renderer code.
namespace Phoenix {

namespace {
    UIListener addListener(GameObject* go, UIEventType type, UICallback callback){
        UIListener handle;
        auto* widget = selectableOf(go);
        if (!widget || !callback) return handle;

        handle.objectUid = go->getUID();
        handle.event = (int)type;
        handle.id = widget->delegateFor(type).add(std::move(callback));
        return handle;
    }

    GameObject* findByUid(GameObject* node, uint32_t uid){
        if (!node) return nullptr;
        if (node->getUID() == uid) return node;
        for (GameObject* child : node->getChildren())
            if (GameObject* found = findByUid(child, uid)) return found;
        return nullptr;
    }

    ComponentSelectable* widget(GameObject* go){ return selectableOf(go); }
}

UIListener UI::OnClick(GameObject* b, UICallback cb){ return addListener(b, UIEventType::Click, std::move(cb)); }
UIListener UI::OnPress(GameObject* b, UICallback cb){ return addListener(b, UIEventType::Press, std::move(cb)); }
UIListener UI::OnRelease(GameObject* b, UICallback cb){ return addListener(b, UIEventType::Release, std::move(cb)); }
UIListener UI::OnHoverEnter(GameObject* b, UICallback cb){ return addListener(b, UIEventType::HoverEnter, std::move(cb)); }
UIListener UI::OnHoverExit(GameObject* b, UICallback cb){ return addListener(b, UIEventType::HoverExit, std::move(cb)); }

UIListener UI::OnToggled(GameObject* go, std::function<void(bool)> cb){
    UIListener handle;
    auto* box = go ? go->getComponent<ComponentCheckBox>() : nullptr;
    if (!box || !cb) return handle;

    handle.objectUid = go->getUID();
    handle.event = (int)UIEventType::ValueChanged;
    handle.id = box->onValueChanged.add(std::move(cb));
    return handle;
}

UIListener UI::OnValueChanged(GameObject* go, std::function<void(float)> cb){
    UIListener handle;
    auto* slider = go ? go->getComponent<ComponentSlider>() : nullptr;
    if (!slider || !cb) return handle;

    handle.objectUid = go->getUID();
    handle.event = (int)UIEventType::ValueChanged;
    handle.id = slider->onValueChanged.add(std::move(cb));
    return handle;
}

UIListener UI::OnTextChanged(GameObject* go, std::function<void(const std::string&)> cb){
    UIListener handle;
    auto* input = go ? go->getComponent<ComponentInputBox>() : nullptr;
    if (!input || !cb) return handle;

    handle.objectUid = go->getUID();
    handle.event = (int)UIEventType::ValueChanged;
    handle.id = input->onValueChanged.add(std::move(cb));
    return handle;
}

UIListener UI::OnSubmit(GameObject* go, std::function<void(const std::string&)> cb){
    UIListener handle;
    auto* input = go ? go->getComponent<ComponentInputBox>() : nullptr;
    if (!input || !cb) return handle;

    handle.objectUid = go->getUID();
    handle.event = (int)UIEventType::Submit;
    handle.id = input->onSubmit.add(std::move(cb));
    return handle;
}

void UI::RemoveListener(const UIListener& listener){
    if (!listener.valid() || !app || !app->getRuntimeCore()) return;
    SceneGraph* scene = app->getRuntimeCore()->getActiveModuleScene();
    if (!scene) return;

    // The button may be gone already; its listeners went with it.
    GameObject* go = findByUid(scene->getRoot(), listener.objectUid);
    if (!go) return;

    if ((UIEventType)listener.event == UIEventType::ValueChanged){
        if (auto* box = go->getComponent<ComponentCheckBox>()) box->onValueChanged.remove(listener.id);
        if (auto* slider = go->getComponent<ComponentSlider>()) slider->onValueChanged.remove(listener.id);
        if (auto* input = go->getComponent<ComponentInputBox>()) input->onValueChanged.remove(listener.id);
    }
    else if ((UIEventType)listener.event == UIEventType::Submit){
        if (auto* input = go->getComponent<ComponentInputBox>()) input->onSubmit.remove(listener.id);
    }
    else if (auto* w = widget(go)){
        w->delegateFor((UIEventType)listener.event).remove(listener.id);
    }
}

void UI::RemoveAllListeners(GameObject* go){
    if (auto* w = widget(go)) w->clearListeners();
}

bool UI::WasClicked(GameObject* go){ auto* w = widget(go); return w && w->clicked; }
bool UI::IsHovered(GameObject* go){ auto* w = widget(go); return w && w->hovered; }
bool UI::IsPressed(GameObject* go){ auto* w = widget(go); return w && w->isHeld(); }
bool UI::IsFocused(GameObject* go){ auto* w = widget(go); return w && w->focused; }

bool UI::IsPointerOverUI(){
    return app && app->getUI() && app->getUI()->isPointerOverUI();
}

void UI::SetInteractable(GameObject* go, bool interactable){ if (auto* w = widget(go)) w->interactable = interactable; }
bool UI::IsInteractable(GameObject* go){ auto* w = widget(go); return w && w->interactable; }

void UI::SetVisible(GameObject* go, bool visible){
    if (auto* t = go ? go->getComponent<ComponentTransform2D>() : nullptr) t->visible = visible;
}

bool UI::IsVisible(GameObject* go){
    auto* t = go ? go->getComponent<ComponentTransform2D>() : nullptr;
    return t && t->visible;
}

void UI::SetText(GameObject* go, const std::string& text){
    if (auto* l = go ? go->getComponent<ComponentLabel>() : nullptr) l->text = text;
}

std::string UI::GetText(GameObject* go){
    auto* l = go ? go->getComponent<ComponentLabel>() : nullptr;
    return l ? l->text : std::string();
}

void UI::SetTextColor(GameObject* go, Color color){
    if (auto* l = go ? go->getComponent<ComponentLabel>() : nullptr) l->color = Vector4(color.x, color.y, color.z, color.w);
}

void UI::SetImageTint(GameObject* go, Color color){
    if (auto* i = go ? go->getComponent<ComponentImage>() : nullptr) i->tint = Vector4(color.x, color.y, color.z, color.w);
}

std::string UI::GetInputText(GameObject* go){
    auto* input = go ? go->getComponent<ComponentInputBox>() : nullptr;
    return input ? input->text : std::string();
}

void UI::SetInputText(GameObject* go, const std::string& text){
    if (auto* input = go ? go->getComponent<ComponentInputBox>() : nullptr) input->setText(text);
}

bool UI::IsTypingText(){
    return app && app->getUI() && app->getUI()->isTextInputActive();
}

void UI::SetChecked(GameObject* go, bool checked){
    auto* box = go ? go->getComponent<ComponentCheckBox>() : nullptr;
    if (!box) return;
    box->checked = checked;
    // Selecting a radio option through script keeps the same "only one chosen" guarantee a click would.
    if (checked && app && app->getRuntimeCore())
        UIRadioGroup::applyExclusivity(app->getRuntimeCore()->getActiveModuleScene(), go);
}

bool UI::IsChecked(GameObject* go){
    auto* box = go ? go->getComponent<ComponentCheckBox>() : nullptr;
    return box && box->checked;
}

GameObject* UI::GetSelectedRadioOption(GameObject* radioGroup){
    if (!radioGroup || !app || !app->getRuntimeCore()) return nullptr;
    return UIRadioGroup::findSelected(app->getRuntimeCore()->getActiveModuleScene(), radioGroup);
}

void UI::SetSliderValue(GameObject* go, float value){
    if (auto* s = go ? go->getComponent<ComponentSlider>() : nullptr) s->setValue(value);
}

float UI::GetSliderValue(GameObject* go){
    auto* s = go ? go->getComponent<ComponentSlider>() : nullptr;
    return s ? s->value : 0.f;
}

float UI::GetSliderNormalized(GameObject* go){
    auto* s = go ? go->getComponent<ComponentSlider>() : nullptr;
    return s ? s->getNormalized() : 0.f;
}

void UI::SetSliderRange(GameObject* go, float minValue, float maxValue, bool wholeNumbers){
    auto* s = go ? go->getComponent<ComponentSlider>() : nullptr;
    if (!s) return;
    s->minValue = minValue;
    s->maxValue = maxValue;
    s->wholeNumbers = wholeNumbers;
    s->setValue(s->value);   // re-clamp into the new range
}

void UI::SetProgress(GameObject* go, float value){
    if (auto* b = go ? go->getComponent<ComponentProgressBar>() : nullptr) b->value = value;
}

float UI::GetProgress(GameObject* go){
    auto* b = go ? go->getComponent<ComponentProgressBar>() : nullptr;
    return b ? b->value : 0.f;
}

float UI::GetProgressNormalized(GameObject* go){
    auto* b = go ? go->getComponent<ComponentProgressBar>() : nullptr;
    return b ? b->getNormalized() : 0.f;
}

void UI::SetProgressRange(GameObject* go, float minValue, float maxValue){
    if (auto* b = go ? go->getComponent<ComponentProgressBar>() : nullptr){
        b->minValue = minValue;
        b->maxValue = maxValue;
    }
}

void UI::SetProgressColor(GameObject* go, Color fill){
    if (auto* b = go ? go->getComponent<ComponentProgressBar>() : nullptr) b->fillColor = Vector4(fill.x, fill.y, fill.z, fill.w);
}

void UI::SetImageTexture(GameObject* go, const std::string& path){
    if (auto* i = go ? go->getComponent<ComponentImage>() : nullptr) i->texturePath = path;
}

void UI::SetPosition(GameObject* go, Vec2 position){
    if (auto* t = go ? go->getComponent<ComponentTransform2D>() : nullptr) t->position = position;
}

Vec2 UI::GetSize(GameObject* go){
    auto* t = go ? go->getComponent<ComponentTransform2D>() : nullptr;
    return t ? Vec2(t->size.x, t->size.y) : Vec2(0.f, 0.f);
}

Vec2 UI::GetPosition(GameObject* go){
    auto* t = go ? go->getComponent<ComponentTransform2D>() : nullptr;
    return t ? t->position : Vec2(0.f, 0.f);
}

void UI::SetSize(GameObject* go, Vec2 size){
    if (auto* t = go ? go->getComponent<ComponentTransform2D>() : nullptr) t->size = size;
}

bool UI::WorldToViewport(Vec3 world, Vec2& out){
    // Same view and projection the game view renders with: the active camera's world matrix inverted, and its
    // perspective at the aspect ratio RuntimeCore keeps on ModuleCamera (inline data only - see the note above).
    ModuleCamera* mc = app ? app->getCamera() : nullptr;
    GameObject* camGo = mc ? mc->getActiveCamera() : nullptr;
    ComponentCamera* cam = camGo ? camGo->getComponent<ComponentCamera>() : nullptr;
    if (!cam || !camGo->getTransform()) return false;
    const Matrix view = camGo->getTransform()->getGlobalMatrix().Invert();
    const float aspect = mc->aspectRatio > 0.f ? mc->aspectRatio : 16.f / 9.f;
    const Matrix proj = Matrix::CreatePerspectiveFieldOfView(cam->getFOV(), aspect, cam->getNearPlane(), cam->getFarPlane());
    const Vector4 clip = Vector4::Transform(Vector4(world.x, world.y, world.z, 1.f), view * proj);
    if (clip.w <= 1e-4f) return false;   // behind the camera
    const float ndcX = clip.x / clip.w, ndcY = clip.y / clip.w, ndcZ = clip.z / clip.w;
    out = Vec2(ndcX * 0.5f + 0.5f, 0.5f - ndcY * 0.5f);
    return ndcZ <= 1.f;
}

bool UI::GetGamePointer(Vec2& out){
    ModuleUI* ui = app ? app->getUI() : nullptr;
    if (!ui){ out = Vec2(0.5f, 0.5f); return false; }
    Vector2 p;
    const bool valid = ui->getGamePointer(p);
    out = Vec2(p.x, p.y);
    return valid;
}

float UI::GetMouseWheel(){
    ModuleUI* ui = app ? app->getUI() : nullptr;
    return ui ? ui->getGameWheel() : 0.f;
}

bool UI::ScreenToRay(Vec2 viewport, Vec3& origin, Vec3& direction){
    // Same camera as WorldToViewport, run backwards: NDC at the near and far planes through the inverse view-proj.
    ModuleCamera* mc = app ? app->getCamera() : nullptr;
    GameObject* camGo = mc ? mc->getActiveCamera() : nullptr;
    ComponentCamera* cam = camGo ? camGo->getComponent<ComponentCamera>() : nullptr;
    if (!cam || !camGo->getTransform()) return false;
    const Matrix view = camGo->getTransform()->getGlobalMatrix().Invert();
    const float aspect = mc->aspectRatio > 0.f ? mc->aspectRatio : 16.f / 9.f;
    const Matrix proj = Matrix::CreatePerspectiveFieldOfView(cam->getFOV(), aspect, cam->getNearPlane(), cam->getFarPlane());
    const Matrix inv = (view * proj).Invert();
    const float ndcX = viewport.x * 2.f - 1.f, ndcY = 1.f - viewport.y * 2.f;
    const Vector4 n = Vector4::Transform(Vector4(ndcX, ndcY, 0.f, 1.f), inv);
    const Vector4 f = Vector4::Transform(Vector4(ndcX, ndcY, 1.f, 1.f), inv);
    if (std::abs(n.w) < 1e-8f || std::abs(f.w) < 1e-8f) return false;
    const Vector3 nearP(n.x / n.w, n.y / n.w, n.z / n.w), farP(f.x / f.w, f.y / f.w, f.z / f.w);
    Vector3 dir = farP - nearP;
    if (dir.LengthSquared() < 1e-12f) return false;
    dir.Normalize();
    origin = Vec3(nearP.x, nearP.y, nearP.z);
    direction = Vec3(dir.x, dir.y, dir.z);
    return true;
}

bool UI::WorldToCanvas(GameObject* go, Vec3 world, Vec2& out){
    // The canvas this widget is laid out in: canvas units = screen pixels / scale factor, and with "scale with
    // screen size" the canvas height only depends on the aspect ratio (ComponentCanvas::getScaleFactor, inlined).
    ComponentCanvas* canvas = nullptr;
    for (GameObject* n = go; n && !canvas; n = n->getParent()) canvas = n->getComponent<ComponentCanvas>();
    ModuleCamera* mc = app ? app->getCamera() : nullptr;
    if (!canvas || !mc) return false;
    Vec2 vp;
    const bool visible = WorldToViewport(world, vp);
    const float aspect = mc->aspectRatio > 0.f ? mc->aspectRatio : 16.f / 9.f;
    float w, h;
    if (canvas->scaleMode == ComponentCanvas::ScaleMode::ScaleWithScreenSize){
        const float refW = std::max(1.f, canvas->referenceResolution.x);
        const float refH = std::max(1.f, canvas->referenceResolution.y);
        const float m = std::clamp(canvas->matchWidthOrHeight, 0.f, 1.f);
        h = std::pow(refW, 1.f - m) * std::pow(refH, m) * std::pow(aspect, -(1.f - m));
        w = h * aspect;
    } else {
        // Constant pixel size: canvas units are pixels; without the window size, assume the reference height.
        h = std::max(1.f, canvas->referenceResolution.y);
        w = h * aspect;
    }
    out = Vec2(vp.x * w, vp.y * h);
    return visible;
}

} // namespace Phoenix
