#pragma once
#include "Component.h"
#include "Globals.h"
#include <vector>
#include <string>
#include <deque>

// CPU trail-renderer component.
// Samples the owner's world position over time (recording a new point once it
// has moved at least `minPointDistance`), ages points out after `duration`,
// and builds a camera-facing ribbon mesh each frame — optionally smoothing the
// path between sampled points with a Centripetal Catmull-Rom spline so corners
// don't look "weird".
// Rendering is handled by TrailPass/TrailPipeline: a small dedicated forward
// pass that uploads the CPU-generated vertex buffer through a ring buffer and
// draws a triangle strip with alpha or additive blending.
class ComponentTrail : public Component {
public:
    enum class BlendMode {
        Alpha = 0,
        Additive = 1,
    };

    enum class TextureMode {
        Stretch = 0, // U spans 0..1 across the whole trail
        Repeat = 1, // U tiles based on arclength / width
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

    // ---- Generation properties----
    bool enabled = true;
    bool emitting = true;
    float duration = 1.0f; // seconds of life for each generated point
    float minPointDistance = 0.05f; // new point spawned once moved at least this far
    float width = 0.25f; // distance between the two ribbon edges

    // ---- Smoothing/ "Centripetal Catmull-Rom") ----
    bool useCatmullRom = true;
    float catmullRomAlpha = 0.5f; // 0 = uniform, 0.5 = centripetal, 1 = chordal
    int subdivisions = 8; // interpolated segments generated between each pair of points

    // ---- Over-lifetime properties----
    Vector4 startColor = Vector4(1.f, 1.f, 1.f, 1.f); // newest point (at the emitter)
    Vector4 endColor = Vector4(1.f, 1.f, 1.f, 0.f); // oldest point (about to vanish)
    float startWidthMul = 1.f;
    float endWidthMul = 0.f;

    // ---- Render ----
    std::string texturePath;
    BlendMode blendMode = BlendMode::Alpha;
    TextureMode textureMode = TextureMode::Stretch;
    int layer = 0;

    // ---- Built-in demo motion (test helper) ----
    // Drives the owner transform along a swinging arc so the trail can be exercised
    // without requiring an external asset or a scripting setup.
    bool demoMotion = false;
    float demoRadius = 1.5f;
    float demoSpeed = 3.0f; // swing speed (rad/s)
    Vector3 demoCenter = Vector3(0.f, 1.f, 0.f); // local-space orbit centre offset

    void clear() { m_points.clear(); }

    // Builds the ribbon mesh for the current frame. `camPos` is used to derive
    // a camera-facing perpendicular at each path point.
    // Returns false when there are fewer than 2 points (nothing to draw).
    bool buildMesh(const Vector3& camPos, std::vector<TrailVertex>& outVertices) const;

private:
    struct TrailPoint {
        Vector3 position;
        float age = 0.f; // seconds since recorded
    };

    // Centripetal Catmull-Rom evaluation between p1 and p2 (p0/p3 are neighbouring
    // control points, extended at path ends). Uses curve.a/b/c/d coefficients, t in [0,1].
    static Vector3 catmullRom(const Vector3& p0, const Vector3& p1,
                              const Vector3& p2, const Vector3& p3,
                              float alpha, float t);

    std::deque<TrailPoint> m_points;
    float m_demoTime = 0.f;
};
