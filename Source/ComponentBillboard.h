#pragma once
#include "Component.h"
#include "Globals.h"

class ComponentBillboard : public Component {
public:
    enum class Alignment {
        Screen = 0,
        World = 1,
        Axial = 2,
    };

    explicit ComponentBillboard(GameObject* owner);
    ~ComponentBillboard() override = default;

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Billboard; }

    std::string texturePath;

    Alignment alignment = Alignment::Screen;

    Vector2 size = Vector2(1.f, 1.f);

    Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f);

    int sheetColumns = 1;
    int sheetRows = 1;

    float framesPerSecond = 0.f;
    bool loop = true;

    bool enabled = true;

    float getCurrentFrame() const { return m_currentFrame; }

private:
    float m_currentFrame = 0.f;
};
