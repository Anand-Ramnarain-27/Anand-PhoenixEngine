#include "Globals.h"
#include "UITestScene.h"
#include "Application.h"
#include "ModuleCamera.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "HotReloadManager.h"
#include "ComponentFactory.h"
#include "ComponentTransform.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentScript.h"
#include "ComponentTransform2D.h"
#include "ComponentCanvas.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentButton.h"
#include "ComponentProgressBar.h"
#include "ComponentCheckBox.h"
#include "ComponentSlider.h"
#include "ComponentInputBox.h"
#include <algorithm>

namespace {
    template<typename T>
    T* add(GameObject* go, Component::Type type){
        go->addComponent(ComponentFactory::CreateComponent(type, go));
        return go->getComponent<T>();
    }

    struct Layout {
        Vector2 anchorMin, anchorMax, pivot, position, size;
    };

    ComponentTransform2D* place(GameObject* go, const Layout& l){
        auto* t = add<ComponentTransform2D>(go, Component::Type::Transform2D);
        t->anchorMin = l.anchorMin;
        t->anchorMax = l.anchorMax;
        t->pivot = l.pivot;
        t->position = l.position;
        t->size = l.size;
        return t;
    }

    // Anchor at one point with the pivot on the same point, so `position` is the distance from that edge/corner.
    Layout pinned(Vector2 anchor, Vector2 position, Vector2 size){
        return { anchor, anchor, anchor, position, size };
    }

    // Fills its parent completely.
    Layout fill(){
        return { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, Vector2::Zero, Vector2::Zero };
    }

    GameObject* image(SceneGraph* scene, GameObject* parent, const char* name, const Layout& l, Vector4 color){
        GameObject* go = scene->createGameObject(name, parent);
        place(go, l);
        add<ComponentImage>(go, Component::Type::Image)->tint = color;
        return go;
    }

    GameObject* bar(SceneGraph* scene, GameObject* parent, const char* name, const Layout& l, float value,
                    ComponentProgressBar::FillDirection direction, Vector4 fill){
        GameObject* go = scene->createGameObject(name, parent);
        place(go, l);
        auto* b = add<ComponentProgressBar>(go, Component::Type::ProgressBar);
        b->value = value;
        b->direction = direction;
        b->fillColor = fill;
        return go;
    }

    GameObject* checkbox(SceneGraph* scene, GameObject* parent, const char* name, const Layout& l, const char* text, bool checked){
        GameObject* go = scene->createGameObject(name, parent);
        place(go, l);
        add<ComponentCheckBox>(go, Component::Type::CheckBox)->checked = checked;
        return go;
    }

    GameObject* slider(SceneGraph* scene, GameObject* parent, const char* name, const Layout& l, float value){
        GameObject* go = scene->createGameObject(name, parent);
        place(go, l);
        auto* s = add<ComponentSlider>(go, Component::Type::Slider);
        s->minValue = 0.f;
        s->maxValue = 100.f;
        s->wholeNumbers = true;
        s->value = value;
        return go;
    }

    GameObject* input(SceneGraph* scene, GameObject* parent, const char* name, const Layout& l, const char* placeholder,
                      ComponentInputBox::ContentType type = ComponentInputBox::ContentType::Standard){
        GameObject* go = scene->createGameObject(name, parent);
        place(go, l);
        auto* box = add<ComponentInputBox>(go, Component::Type::InputBox);
        box->placeholder = placeholder;
        box->contentType = type;
        return go;
    }

    GameObject* label(SceneGraph* scene, GameObject* parent, const char* name, const Layout& l, const char* text,
                      float size, Vector4 color, ComponentLabel::HAlign h = ComponentLabel::HAlign::Center,
                      ComponentLabel::VAlign v = ComponentLabel::VAlign::Middle){
        GameObject* go = scene->createGameObject(name, parent);
        place(go, l);
        auto* c = add<ComponentLabel>(go, Component::Type::Label);
        c->text = text;
        c->fontSize = size;
        c->color = color;
        c->hAlign = h;
        c->vAlign = v;
        return go;
    }

    GameObject* button(SceneGraph* scene, GameObject* parent, const char* name, const char* text,
                       Vector2 position, Vector4 color, bool interactable){
        GameObject* go = image(scene, parent, name, { { .5f, 0.f }, { .5f, 0.f }, { .5f, 0.f }, position, { 420.f, 72.f } }, color);
        add<ComponentButton>(go, Component::Type::Button)->interactable = interactable;
        label(scene, go, "Text", fill(), text, 32.f, Vector4(1.f, 1.f, 1.f, 1.f));
        return go;
    }
}

void CreateUITestScene(SceneGraph* scene, HotReloadManager* hotReload){
    if (!scene) return;

    // ---- 3D: something to look at, so UI-over-scene compositing is visible ----
    GameObject* camGO = scene->createGameObject("UI Test Camera");
    camGO->getTransform()->position = Vector3(0.f, 2.f, 7.f);
    camGO->getTransform()->rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitX, -0.2f);
    camGO->getTransform()->markDirty();
    add<ComponentCamera>(camGO, Component::Type::Camera)->setMainCamera(true);

    GameObject* sun = scene->createGameObject("UI Test Light");
    add<ComponentDirectionalLight>(sun, Component::Type::DirectionalLight);

    // ---- UI ----
    GameObject* canvasGO = scene->createGameObject("UI Test Canvas");
    add<ComponentCanvas>(canvasGO, Component::Type::Canvas);

    const Vector2 topLeft(0.f, 0.f), topRight(1.f, 0.f), bottomLeft(0.f, 1.f), bottomRight(1.f, 1.f), center(.5f, .5f);
    const Vector4 white(1.f, 1.f, 1.f, 1.f), grey(.8f, .8f, .8f, 1.f), gold(1.f, .82f, .2f, 1.f);

    // Panel pinned to the top-left, holding the buttons.
    GameObject* panel = image(scene, canvasGO, "Panel", pinned(topLeft, { 40.f, 40.f }, { 620.f, 590.f }), Vector4(0.f, 0.f, 0.f, .6f));
    label(scene, panel, "Title", { { 0.f, 0.f }, { 1.f, 0.f }, { .5f, 0.f }, { 0.f, 16.f }, { 0.f, 56.f } },
          "Phoenix UI Test", 44.f, gold);
    label(scene, panel, "Hint", { { 0.f, 0.f }, { 1.f, 0.f }, { .5f, 0.f }, { 0.f, 84.f }, { -40.f, 70.f } },
          "Press Play, then hover, click and\nTab through the buttons", 24.f, grey);

    GameObject* clickMe = button(scene, panel, "Button Click Me", "Click me", { 0.f, 190.f }, Vector4(.24f, .36f, .68f, 1.f), true);
    button(scene, panel, "Button Second", "Second button", { 0.f, 280.f }, Vector4(.20f, .55f, .35f, 1.f), true);
    button(scene, panel, "Button Disabled", "Disabled", { 0.f, 370.f }, Vector4(.6f, .3f, .3f, 1.f), false);

    // Filled by UIDemoScript, one tenth per click.
    bar(scene, panel, "Click Progress", { { .5f, 0.f }, { .5f, 0.f }, { .5f, 0.f }, { 0.f, 462.f }, { 420.f, 26.f } },
        0.f, ComponentProgressBar::FillDirection::LeftToRight, Vector4(.95f, .75f, .2f, 1.f));

    // Count clicks on screen when the script DLL is loaded.
    bool scripted = false;
    if (hotReload){
        for (const std::string& name : hotReload->getRegisteredClassNames()){
            if (name != "UIDemoScript") continue;
            auto comp = ComponentFactory::CreateComponent(Component::Type::Script, clickMe);
            static_cast<ComponentScript*>(comp.get())->setScriptClass(name, hotReload);
            clickMe->addComponent(std::move(comp));
            scripted = true;
        }
    }
    label(scene, panel, "Script Note", { { 0.f, 1.f }, { 1.f, 1.f }, { .5f, 1.f }, { 0.f, -14.f }, { -40.f, 40.f } },
          scripted ? "UIDemoScript attached: clicks are counted" : "No UIDemoScript loaded (build GameScript)", 20.f,
          scripted ? Vector4(.5f, 1.f, .5f, 1.f) : Vector4(1.f, .6f, .4f, 1.f));

    // Below the panel: a checkbox (its row includes the text, so the text toggles it) and a slider.
    GameObject* toggle = checkbox(scene, canvasGO, "Test Checkbox", pinned(topLeft, { 40.f, 660.f }, { 420.f, 44.f }), "", true);
    label(scene, toggle, "Checkbox Text", { { 0.f, 0.f }, { 1.f, 1.f }, { .5f, .5f }, { 27.f, 0.f }, { -54.f, 0.f } },
          "Enable second button", 30.f, white, ComponentLabel::HAlign::Left);
    slider(scene, canvasGO, "Test Slider", pinned(topLeft, { 40.f, 730.f }, { 420.f, 36.f }), 70.f);
    label(scene, canvasGO, "Slider Text", pinned(topLeft, { 480.f, 730.f }, { 300.f, 36.f }), "<- sets the Health Bar", 26.f, grey,
          ComponentLabel::HAlign::Left);

    // Text fields: free text (echoed by UIDemoScript) and a numbers-only one.
    input(scene, canvasGO, "Test Input", pinned(topLeft, { 40.f, 790.f }, { 420.f, 48.f }), "Type here...");
    label(scene, canvasGO, "Echo Label", pinned(topLeft, { 480.f, 790.f }, { 460.f, 48.f }), "Echo: (needs UIDemoScript)", 26.f, grey,
          ComponentLabel::HAlign::Left);
    input(scene, canvasGO, "Number Input", pinned(topLeft, { 40.f, 850.f }, { 420.f, 48.f }), "Numbers only",
          ComponentInputBox::ContentType::Integer);

    // Corner anchors: these stay glued to the screen corners at any resolution.
    label(scene, canvasGO, "Anchor TR", pinned(topRight, { -40.f, 40.f }, { 360.f, 50.f }), "Top Right anchor", 30.f, white,
          ComponentLabel::HAlign::Right, ComponentLabel::VAlign::Top);
    label(scene, canvasGO, "Anchor BL", pinned(bottomLeft, { 40.f, -110.f }, { 360.f, 50.f }), "Bottom Left anchor", 30.f, white,
          ComponentLabel::HAlign::Left, ComponentLabel::VAlign::Bottom);
    label(scene, canvasGO, "Anchor BR", pinned(bottomRight, { -40.f, -110.f }, { 360.f, 50.f }), "Bottom Right anchor", 30.f, white,
          ComponentLabel::HAlign::Right, ComponentLabel::VAlign::Bottom);

    // Centre crosshair, built from two thin images.
    image(scene, canvasGO, "Crosshair H", pinned(center, Vector2::Zero, { 48.f, 4.f }), white);
    image(scene, canvasGO, "Crosshair V", pinned(center, Vector2::Zero, { 4.f, 48.f }), white);

    // Rotation about the pivot, off-centre.
    GameObject* spinner = image(scene, canvasGO, "Rotated Image", pinned(center, { 360.f, -60.f }, { 170.f, 170.f }), Vector4(1.f, .55f, .1f, 1.f));
    spinner->getComponent<ComponentTransform2D>()->rotation = 30.f;
    label(scene, canvasGO, "Rotated Label", pinned(center, { 360.f, -60.f }, { 200.f, 40.f }), "rotated 30 deg", 26.f, white);

    // Progress bars: horizontal with a text overlay, reversed, and vertical.
    GameObject* health = bar(scene, canvasGO, "Health Bar", pinned({ .5f, 0.f }, { 0.f, 40.f }, { 520.f, 40.f }), 0.7f,
                             ComponentProgressBar::FillDirection::LeftToRight, Vector4(.85f, .2f, .2f, 1.f));
    label(scene, health, "Health Text", fill(), "HP 70 / 100", 26.f, white);

    bar(scene, canvasGO, "Reversed Bar", pinned({ .5f, 0.f }, { 0.f, 96.f }, { 520.f, 22.f }), 0.35f,
        ComponentProgressBar::FillDirection::RightToLeft, Vector4(.3f, .6f, 1.f, 1.f));

    bar(scene, canvasGO, "Vertical Bar", pinned({ 1.f, .5f }, { -90.f, 0.f }, { 44.f, 320.f }), 0.45f,
        ComponentProgressBar::FillDirection::BottomToTop, Vector4(.4f, .85f, .4f, 1.f));

    // Stretched along the bottom: anchors span the full width, size.x insets it.
    GameObject* bar = image(scene, canvasGO, "Stretched Bar", { { 0.f, 1.f }, { 1.f, 1.f }, { .5f, 1.f }, { 0.f, -20.f }, { -500.f, 64.f } },
                            Vector4(.15f, .15f, .25f, .85f));
    label(scene, bar, "Bar Text", fill(), "Stretched bottom bar (anchors 0..1)", 28.f, white);

    LOG("[UITest] Created camera, light and UI test canvas%s.", scripted ? " (UIDemoScript attached)" : "");
}
