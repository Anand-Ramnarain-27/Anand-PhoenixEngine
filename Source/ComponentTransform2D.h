#pragma once
#include "Component.h"
#include "Globals.h"

struct UIRect {
    Vector2 min = Vector2::Zero;
    Vector2 max = Vector2::Zero;

    Vector2 size() const { return max - min; }
    Vector2 center() const { return (min + max) * 0.5f; }
    bool contains(const Vector2& p) const { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }
};

// 2D layout for UI widgets (a simplified Unity RectTransform). Coordinates are canvas units with the
// origin at the top-left of the parent rect and +Y pointing down.
//
//   anchorMin/anchorMax : corners of the anchor box inside the parent rect, normalized 0..1.
//                         Equal anchors pin the widget to a point; different anchors stretch it.
//   position            : offset of the pivot from its anchor reference point.
//   size                : the widget size when anchors are equal, otherwise an extra amount added
//                         to the stretched anchor-box size (negative values inset it).
//   pivot               : normalized point of the rect that position, rotation and scale act around.
//
// Rotation is applied when the widget draws; children are laid out in the unrotated rect.
class ComponentTransform2D : public Component {
public:
    explicit ComponentTransform2D(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Transform2D; }

    Vector2 position = Vector2(0.f, 0.f);
    Vector2 size = Vector2(100.f, 100.f);
    Vector2 anchorMin = Vector2(0.5f, 0.5f);
    Vector2 anchorMax = Vector2(0.5f, 0.5f);
    Vector2 pivot = Vector2(0.5f, 0.5f);
    float rotation = 0.f;
    Vector2 scale = Vector2(1.f, 1.f);
    bool visible = true;

    // Clips every descendant (recursively, until a nested mask narrows it further) to this rect. Useful for
    // scroll views and panels with overflowing content. Ignored when the widget is rotated, since the clip is
    // an axis-aligned screen rectangle.
    bool maskChildren = false;

    void computeLayout(const UIRect& parent);

    const UIRect& getRect() const { return m_rect; }
    Vector2 getPivotPosition() const { return m_pivotPos; }

private:
    UIRect m_rect;
    Vector2 m_pivotPos = Vector2::Zero;
};
