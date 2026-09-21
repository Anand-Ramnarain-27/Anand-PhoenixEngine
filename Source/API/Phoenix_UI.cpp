#include "Globals.h"
#include "API/Phoenix_UI.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ModuleUI.h"
#include "ComponentButton.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentTransform2D.h"

// Everything here works on component data and inline code only: GameScript.dll shares just the `app` pointer
// with the engine, so it cannot call into renderer code.
namespace Phoenix {

namespace {
    UIListener addListener(GameObject* go, UIEventType type, UICallback callback){
        UIListener handle;
        auto* button = go ? go->getComponent<ComponentButton>() : nullptr;
        if (!button || !callback) return handle;

        handle.objectUid = go->getUID();
        handle.event = (int)type;
        handle.id = button->delegateFor(type).add(std::move(callback));
        return handle;
    }

    GameObject* findByUid(GameObject* node, uint32_t uid){
        if (!node) return nullptr;
        if (node->getUID() == uid) return node;
        for (GameObject* child : node->getChildren())
            if (GameObject* found = findByUid(child, uid)) return found;
        return nullptr;
    }

    ComponentButton* button(GameObject* go){ return go ? go->getComponent<ComponentButton>() : nullptr; }
}

UIListener UI::OnClick(GameObject* b, UICallback cb){ return addListener(b, UIEventType::Click, std::move(cb)); }
UIListener UI::OnPress(GameObject* b, UICallback cb){ return addListener(b, UIEventType::Press, std::move(cb)); }
UIListener UI::OnRelease(GameObject* b, UICallback cb){ return addListener(b, UIEventType::Release, std::move(cb)); }
UIListener UI::OnHoverEnter(GameObject* b, UICallback cb){ return addListener(b, UIEventType::HoverEnter, std::move(cb)); }
UIListener UI::OnHoverExit(GameObject* b, UICallback cb){ return addListener(b, UIEventType::HoverExit, std::move(cb)); }

void UI::RemoveListener(const UIListener& listener){
    if (!listener.valid() || !app || !app->getRuntimeCore()) return;
    SceneGraph* scene = app->getRuntimeCore()->getActiveModuleScene();
    if (!scene) return;

    // The button may be gone already; its listeners went with it.
    GameObject* go = findByUid(scene->getRoot(), listener.objectUid);
    if (auto* b = button(go))
        b->delegateFor((UIEventType)listener.event).remove(listener.id);
}

void UI::RemoveAllListeners(GameObject* go){
    if (auto* b = button(go)) b->clearListeners();
}

bool UI::WasClicked(GameObject* go){ auto* b = button(go); return b && b->clicked; }
bool UI::IsHovered(GameObject* go){ auto* b = button(go); return b && b->hovered; }
bool UI::IsPressed(GameObject* go){ auto* b = button(go); return b && b->isHeld(); }
bool UI::IsFocused(GameObject* go){ auto* b = button(go); return b && b->focused; }

bool UI::IsPointerOverUI(){
    return app && app->getUI() && app->getUI()->isPointerOverUI();
}

void UI::SetInteractable(GameObject* go, bool interactable){ if (auto* b = button(go)) b->interactable = interactable; }
bool UI::IsInteractable(GameObject* go){ auto* b = button(go); return b && b->interactable; }

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

void UI::SetImageTexture(GameObject* go, const std::string& path){
    if (auto* i = go ? go->getComponent<ComponentImage>() : nullptr) i->texturePath = path;
}

} // namespace Phoenix
