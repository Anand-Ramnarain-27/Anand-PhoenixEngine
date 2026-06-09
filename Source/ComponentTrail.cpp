#include "Globals.h"
#include "ComponentTrail.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "AssetBrowserPanel.h"
#include "AssetPickerWidget.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

ComponentTrail::ComponentTrail(GameObject* owner) : Component(owner) {}

// Centripetal Catmull-Rom — reformulation via knot intervals:
//   t0=0, t1=t0+|p0p1|^a, t2=t1+|p1p2|^a, t3=t2+|p2p3|^a
//   m1 = (t2-t1) * ((p1-p0)/(t1-t0) - (p2-p0)/(t2-t0) + (p2-p1)/(t2-t1))
//   m2 = (t2-t1) * ((p2-p1)/(t2-t1) - (p3-p1)/(t3-t1) + (p3-p2)/(t3-t2))
//   curve.a = 2*(p1-p2)+m1+m2 ; curve.b = -3*(p1-p2)-m1-m1-m2
//   curve.c = m1 ; curve.d = p1
//   point(t) = a*t^3 + b*t^2 + c*t + d ,  t in [0,1]
Vector3 ComponentTrail::catmullRom(const Vector3& p0, const Vector3& p1,
                                   const Vector3& p2, const Vector3& p3,
                                   float alpha, float t){
    auto safeDist = [](const Vector3& a, const Vector3& b) {
        return std::max(1e-4f, Vector3::Distance(a, b));
    };

    const float t0 = 0.0f;
    const float t1 = t0 + std::pow(safeDist(p0, p1), alpha);
    const float t2 = t1 + std::pow(safeDist(p1, p2), alpha);
    const float t3 = t2 + std::pow(safeDist(p2, p3), alpha);

    const float d10 = std::max(1e-4f, t1 - t0);
    const float d20 = std::max(1e-4f, t2 - t0);
    const float d21 = std::max(1e-4f, t2 - t1);
    const float d31 = std::max(1e-4f, t3 - t1);
    const float d32 = std::max(1e-4f, t3 - t2);

    Vector3 m1 = (t2 - t1) * ((p1 - p0) / d10 - (p2 - p0) / d20 + (p2 - p1) / d21);
    Vector3 m2 = (t2 - t1) * ((p2 - p1) / d21 - (p3 - p1) / d31 + (p3 - p2) / d32);

    Vector3 a = 2.0f * (p1 - p2) + m1 + m2;
    Vector3 b = -3.0f * (p1 - p2) - m1 - m1 - m2;
    Vector3 c = m1;
    Vector3 d = p1;

    return a * (t * t * t) + b * (t * t) + c * t + d;
}

void ComponentTrail::update(float dt){
    if (!enabled) return;

    // Built-in demo motion — swings the owner along an arc so
    // the trail has something to trace without requiring an external asset.
    if (demoMotion && owner) {
        m_demoTime += dt;
        const float angle = std::sin(m_demoTime * demoSpeed) * 1.57079632679f; // +/- 90 degrees swing
        Vector3 offset(std::sin(angle) * demoRadius, 0.f, std::cos(angle) * demoRadius - demoRadius);
        auto* xform = owner->getTransform();
        xform->position = demoCenter + offset;
        xform->rotation = Quaternion::CreateFromYawPitchRoll(angle, 0.f, 0.f);
        xform->markDirty();
    }

    // Age existing points and drop ones past their lifetime (duration).
    for (auto& p : m_points) p.age += dt;
    while (!m_points.empty() && m_points.front().age >= duration)
        m_points.pop_front();

    if (emitting && owner) {
        Vector3 worldPos = owner->getTransform()->getGlobalMatrix().Translation();
        if (m_points.empty() || Vector3::Distance(worldPos, m_points.back().position) >= minPointDistance) {
            m_points.push_back({ worldPos, 0.f });
        }
    }
}

bool ComponentTrail::buildMesh(const Vector3& camPos, std::vector<TrailVertex>& outVertices) const{
    outVertices.clear();
    if (m_points.size() < 2) return false;

    // 1. Resolve the path: either the raw recorded points, or a Centripetal
    //    Catmull-Rom-smoothed sequence with `subdivisions` extra points
    //    inserted between each pair.
    struct PathPoint { Vector3 pos; float t; }; // t = normalised age in [0,1]
    std::vector<PathPoint> path;

    const int n = (int)m_points.size();
    auto ageT = [&](int i) { return std::clamp(m_points[i].age / std::max(0.0001f, duration), 0.f, 1.f); };

    if (useCatmullRom && n >= 3 && subdivisions > 0) {
        path.reserve((n - 1) * (subdivisions + 1) + 1);
        for (int i = 0; i < n - 1; ++i) {
            // Extend by reflecting the boundary segment for the missing control point.
            const Vector3& p1 = m_points[i].position;
            const Vector3& p2 = m_points[i + 1].position;
            Vector3 p0 = (i == 0) ? (2.0f * p1 - p2) : m_points[i - 1].position;
            Vector3 p3 = (i + 2 >= n) ? (2.0f * p2 - p1) : m_points[i + 2].position;

            const int steps = subdivisions + 1;
            for (int s = 0; s < steps; ++s) {
                float t = (float)s / (float)steps;
                Vector3 pos = catmullRom(p0, p1, p2, p3, catmullRomAlpha, t);
                float age = ageT(i) + (ageT(i + 1) - ageT(i)) * t;
                path.push_back({ pos, age });
            }
        }
        path.push_back({ m_points.back().position, ageT(n - 1) });
    } else {
        path.reserve(n);
        for (int i = 0; i < n; ++i)
            path.push_back({ m_points[i].position, ageT(i) });
    }

    if (path.size() < 2) return false;

    // 2. Compute total arclength up-front for both texture modes:
    //    "Repeat" uses arclength / width; "Stretch" maps [0,totalLen] → [0,1].
    std::vector<float> arclen(path.size(), 0.f);
    for (size_t i = 1; i < path.size(); ++i)
        arclen[i] = arclen[i - 1] + Vector3::Distance(path[i].pos, path[i - 1].pos);
    const float totalLen = std::max(1e-4f, arclen.back());

    // 3. Build the camera-facing ribbon: cross the local tangent with the view
    //    direction to get a perpendicular, same trick billboards use.
    outVertices.reserve(path.size() * 6);
    const size_t count = path.size();

    for (size_t i = 0; i + 1 < count; ++i) {
        auto buildSide = [&](size_t idx) -> std::pair<Vector3, Vector3> {
            Vector3 tangent;
            if (idx == 0) tangent = path[1].pos - path[0].pos;
            else if (idx == count - 1) tangent = path[count - 1].pos - path[count - 2].pos;
            else tangent = path[idx + 1].pos - path[idx - 1].pos;
            if (tangent.LengthSquared() < 1e-10f) tangent = Vector3(0.f, 0.f, 1.f);
            tangent.Normalize();

            Vector3 toCam = camPos - path[idx].pos;
            Vector3 perp = tangent.Cross(toCam);
            if (perp.LengthSquared() < 1e-10f) perp = Vector3(1.f, 0.f, 0.f);
            perp.Normalize();

            float wMul = startWidthMul + (endWidthMul - startWidthMul) * path[idx].t;
            float halfW = 0.5f * width * std::max(0.f, wMul);
            return { path[idx].pos - perp * halfW, path[idx].pos + perp * halfW };
        };

        auto [leftA, rightA] = buildSide(i);
        auto [leftB, rightB] = buildSide(i + 1);

        Vector4 colorA = Vector4::Lerp(startColor, endColor, path[i].t);
        Vector4 colorB = Vector4::Lerp(startColor, endColor, path[i + 1].t);

        float uA, uB;
        if (textureMode == TextureMode::Repeat) {
            uA = arclen[i] / std::max(0.0001f, width);
            uB = arclen[i + 1] / std::max(0.0001f, width);
        } else { // Stretch — head (newest, t=0) to tail (oldest, t=1)
            uA = arclen[i] / totalLen;
            uB = arclen[i + 1] / totalLen;
        }

        // Two triangles per quad segment, wound CCW when viewed from `camPos`
        // (NONE-cull pipeline makes winding mostly irrelevant, but keep it tidy).
        TrailVertex v0{ leftA, Vector2(uA, 0.f), colorA };
        TrailVertex v1{ rightA, Vector2(uA, 1.f), colorA };
        TrailVertex v2{ leftB, Vector2(uB, 0.f), colorB };
        TrailVertex v3{ rightB, Vector2(uB, 1.f), colorB };

        outVertices.push_back(v0);
        outVertices.push_back(v1);
        outVertices.push_back(v2);

        outVertices.push_back(v1);
        outVertices.push_back(v3);
        outVertices.push_back(v2);
    }

    return !outVertices.empty();
}

void ComponentTrail::onEditor(){
    ImGui::Checkbox("Enabled##trail", &enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Emitting##trail", &emitting);
    ImGui::SameLine();
    if (ImGui::Button("Clear##trail")) clear();

    ImGui::Separator();
    ImGui::TextUnformatted("Generation");
    ImGui::DragFloat("Duration", &duration, 0.02f, 0.05f, 30.f);
    ImGui::DragFloat("Min point distance", &minPointDistance, 0.005f, 0.001f, 10.f);
    ImGui::DragFloat("Width", &width, 0.01f, 0.001f, 50.f);

    ImGui::Separator();
    ImGui::TextUnformatted("Smoothing (Centripetal Catmull-Rom)");
    ImGui::Checkbox("Use Catmull-Rom spline", &useCatmullRom);
    if (useCatmullRom) {
        ImGui::SliderFloat("Alpha (0=uniform .5=centripetal 1=chordal)", &catmullRomAlpha, 0.f, 1.f);
        ImGui::DragInt("Subdivisions", &subdivisions, 1.f, 0, 32);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Over lifetime");
    ImGui::ColorEdit4("Start colour (newest)", &startColor.x);
    ImGui::ColorEdit4("End colour (oldest)", &endColor.x);
    ImGui::DragFloat("Start width mult.", &startWidthMul, 0.01f, 0.f, 10.f);
    ImGui::DragFloat("End width mult.", &endWidthMul, 0.01f, 0.f, 10.f);

    ImGui::Separator();
    ImGui::TextUnformatted("Render");
    {
        // Unity-style asset picker: click to open a searchable list, or drag from
        // the Asset Browser. Shows the filename with a type tag; full path in tooltip.
        ImGui::TextUnformatted("Texture");
        ImGui::SameLine(90.f);
        AssetPicker::Draw("##trailTexture", texturePath, AssetPicker::kTextures);
    }
    static const char* kBlend[] = { "Alpha", "Additive" };
    int blendIdx = (int)blendMode;
    if (ImGui::Combo("Blend mode", &blendIdx, kBlend, IM_ARRAYSIZE(kBlend)))
        blendMode = (BlendMode)blendIdx;

    static const char* kTexMode[] = { "Stretch", "Repeat" };
    int texModeIdx = (int)textureMode;
    if (ImGui::Combo("Texture mode", &texModeIdx, kTexMode, IM_ARRAYSIZE(kTexMode)))
        textureMode = (TextureMode)texModeIdx;

    ImGui::DragInt("Layer", &layer, 1.f, -100, 100);

    ImGui::Separator();
    ImGui::TextUnformatted("Demo motion (test helper - 'Sword trail' exercise)");
    ImGui::Checkbox("Enable demo swing motion", &demoMotion);
    if (demoMotion) {
        ImGui::DragFloat("Swing radius", &demoRadius, 0.05f, 0.05f, 20.f);
        ImGui::DragFloat("Swing speed", &demoSpeed, 0.05f, 0.05f, 20.f);
        ImGui::DragFloat3("Swing centre", &demoCenter.x, 0.05f);
    }

    ImGui::Separator();
    ImGui::Text("Sample points: %d", (int)m_points.size());
}

void ComponentTrail::onSave(std::string& outJson) const{
    outJson += "\"enabled\":" + std::string(enabled ? "true" : "false") + ",";
    outJson += "\"emitting\":" + std::string(emitting ? "true" : "false") + ",";
    outJson += "\"duration\":" + std::to_string(duration) + ",";
    outJson += "\"minPointDistance\":" + std::to_string(minPointDistance) + ",";
    outJson += "\"width\":" + std::to_string(width) + ",";
    outJson += "\"useCatmullRom\":" + std::string(useCatmullRom ? "true" : "false") + ",";
    outJson += "\"catmullRomAlpha\":" + std::to_string(catmullRomAlpha) + ",";
    outJson += "\"subdivisions\":" + std::to_string(subdivisions) + ",";
    outJson += "\"startColor\":[" + std::to_string(startColor.x) + "," + std::to_string(startColor.y) + "," +
                                     std::to_string(startColor.z) + "," + std::to_string(startColor.w) + "],";
    outJson += "\"endColor\":[" + std::to_string(endColor.x) + "," + std::to_string(endColor.y) + "," +
                                   std::to_string(endColor.z) + "," + std::to_string(endColor.w) + "],";
    outJson += "\"startWidthMul\":" + std::to_string(startWidthMul) + ",";
    outJson += "\"endWidthMul\":" + std::to_string(endWidthMul) + ",";
    outJson += "\"texturePath\":\"" + texturePath + "\",";
    outJson += "\"blendMode\":" + std::to_string((int)blendMode) + ",";
    outJson += "\"textureMode\":" + std::to_string((int)textureMode) + ",";
    outJson += "\"layer\":" + std::to_string(layer) + ",";
    outJson += "\"demoMotion\":" + std::string(demoMotion ? "true" : "false") + ",";
    outJson += "\"demoRadius\":" + std::to_string(demoRadius) + ",";
    outJson += "\"demoSpeed\":" + std::to_string(demoSpeed) + ",";
    outJson += "\"demoCenter\":[" + std::to_string(demoCenter.x) + "," + std::to_string(demoCenter.y) + "," +
                                     std::to_string(demoCenter.z) + "]";
}

void ComponentTrail::onLoad(const std::string& json){
    auto extract = [&](const char* key) -> std::string {
        std::string k = "\"" + std::string(key) + "\":";
        auto pos = json.find(k);
        if (pos == std::string::npos) return {};
        pos += k.size();
        if (json[pos] == '"') {
            ++pos;
            auto end = json.find('"', pos);
            return json.substr(pos, end - pos);
        }
        if (json[pos] == '[') {
            auto end = json.find(']', pos);
            return json.substr(pos, end - pos + 1);
        }
        auto end = json.find_first_of(",}", pos);
        return json.substr(pos, end - pos);
    };

    auto extractArray = [&](const std::string& arr, float* out, int count) {
        if (arr.size() < 2) return;
        std::string inner = arr.substr(1, arr.size() - 2);
        size_t start = 0;
        for (int i = 0; i < count; ++i) {
            size_t comma = inner.find(',', start);
            std::string token = (comma == std::string::npos) ? inner.substr(start)
                                                              : inner.substr(start, comma - start);
            if (!token.empty()) out[i] = std::stof(token);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    };

    auto getBool = [&](const char* key, bool def) { auto v = extract(key); return v.empty() ? def : (v == "true"); };
    auto getFloat = [&](const char* key, float def) { auto v = extract(key); return v.empty() ? def : std::stof(v); };
    auto getInt = [&](const char* key, int def) { auto v = extract(key); return v.empty() ? def : std::stoi(v); };

    enabled = getBool("enabled", true);
    emitting = getBool("emitting", true);
    duration = getFloat("duration", duration);
    minPointDistance = getFloat("minPointDistance", minPointDistance);
    width = getFloat("width", width);
    useCatmullRom = getBool("useCatmullRom", useCatmullRom);
    catmullRomAlpha = getFloat("catmullRomAlpha", catmullRomAlpha);
    subdivisions = getInt("subdivisions", subdivisions);

    if (auto v = extract("startColor"); !v.empty()) extractArray(v, &startColor.x, 4);
    if (auto v = extract("endColor"); !v.empty()) extractArray(v, &endColor.x, 4);

    startWidthMul = getFloat("startWidthMul", startWidthMul);
    endWidthMul = getFloat("endWidthMul", endWidthMul);
    texturePath = extract("texturePath");
    blendMode = (BlendMode)getInt("blendMode", (int)blendMode);
    textureMode = (TextureMode)getInt("textureMode", (int)textureMode);
    layer = getInt("layer", layer);

    demoMotion = getBool("demoMotion", demoMotion);
    demoRadius = getFloat("demoRadius", demoRadius);
    demoSpeed = getFloat("demoSpeed", demoSpeed);
    if (auto v = extract("demoCenter"); !v.empty()) extractArray(v, &demoCenter.x, 3);
}
