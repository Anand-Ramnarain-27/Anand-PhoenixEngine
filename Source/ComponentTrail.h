#pragma once
#include "Component.h"
#include "Globals.h"
#include "CurveWidget.h"
#include <vector>
#include <string>
#include <deque>

class ComponentTrail : public Component {
public:
    enum class BlendMode {
        Alpha = 0,
        Additive = 1,
    };

    enum class TextureMode {
        Stretch = 0,
        Repeat = 1,
    };

    struct TrailVertex {
        Vector3 position;
        Vector2 uv;
        Vector4 color;
    };

    explicit ComponentTrail(GameObject* owner);
    ~ComponentTrail() override = default;

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Trail; }

    bool enabled = true;
    bool emitting = true;
    float duration = 1.0f;
    float minPointDistance = 0.05f;
    float width = 0.25f;
    float maxSegmentAngle = 15.f;

    bool useCatmullRom = true;
    float catmullRomAlpha = 0.5f;
    int subdivisions = 8;

    Vector4 startColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 endColor = Vector4(1.f, 1.f, 1.f, 0.f);
    float startWidthMul = 1.f;
    float endWidthMul = 0.f;
    EaseCurve widthCurve;

    std::string texturePath;
    BlendMode blendMode = BlendMode::Alpha;
    TextureMode textureMode = TextureMode::Stretch;
    int layer = 0;

    bool previewOrbit = false;
    float orbitRadius = 1.5f;
    float orbitSpeed = 2.5f;
    Vector3 orbitCenter;

    void clear(){ m_points.clear(); m_orbitInitialized = false; }

    bool buildMesh(const Vector3& camPos, std::vector<TrailVertex>& outVertices) const;

private:
    struct TrailPoint {
        Vector3 position;
        float age = 0.f;
    };

    static Vector3 catmullRom(const Vector3& p0, const Vector3& p1,
                              const Vector3& p2, const Vector3& p3,
                              float alpha, float t);

    std::deque<TrailPoint> m_points;
    float m_orbitAngle = 0.f;
    bool m_orbitInitialized = false;
};
