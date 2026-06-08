#include "Globals.h"
#include "ComponentParticleSystem.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "AssetBrowserPanel.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDeg2Rad = kPi / 180.f;

    float randRange(std::mt19937& rng, float lo, float hi) {
        if (hi < lo) std::swap(lo, hi);
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(rng);
    }
}

ComponentParticleSystem::ComponentParticleSystem(GameObject* owner) : Component(owner) {}

// Direction sampling per emitter shape (lecture: "Emitter — shape type / shape
// related parameters: angle, radius"). Directions are generated in the emitter's
// local frame (forward = +Y of the owner's transform) then rotated to world space.
Vector3 ComponentParticleSystem::randomEmitDirection(std::mt19937& rng) const {
    Vector3 localDir(0.f, 1.f, 0.f);

    switch (shape) {
    case EmitterShape::Cone: {
        // Random direction within a cone around +Y, half-angle = coneAngleDeg.
        float maxAngle = std::max(0.f, coneAngleDeg) * kDeg2Rad;
        float cosMax   = std::cos(maxAngle);
        float cosTheta = randRange(rng, cosMax, 1.f);
        float sinTheta = std::sqrt(std::max(0.f, 1.f - cosTheta * cosTheta));
        float phi      = randRange(rng, 0.f, 2.f * kPi);
        localDir = Vector3(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));
        break;
    }
    case EmitterShape::Sphere: {
        // Uniform direction over the full sphere.
        float cosTheta = randRange(rng, -1.f, 1.f);
        float sinTheta = std::sqrt(std::max(0.f, 1.f - cosTheta * cosTheta));
        float phi      = randRange(rng, 0.f, 2.f * kPi);
        localDir = Vector3(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));
        break;
    }
    case EmitterShape::Box:
    case EmitterShape::Point:
    default:
        localDir = Vector3(0.f, 1.f, 0.f);
        break;
    }

    if (!owner) return localDir;
    const Matrix& world = owner->getTransform()->getGlobalMatrix();
    Vector3 worldDir = Vector3::TransformNormal(localDir, world);
    worldDir.Normalize();
    return worldDir;
}

// Position sampling within the emitter shape, in world space, around the owner origin.
Vector3 ComponentParticleSystem::randomEmitPosition(std::mt19937& rng) const {
    Vector3 localOffset(0.f, 0.f, 0.f);

    switch (shape) {
    case EmitterShape::Box:
        localOffset = Vector3(randRange(rng, -shapeRadius, shapeRadius),
                              randRange(rng, -shapeRadius, shapeRadius),
                              randRange(rng, -shapeRadius, shapeRadius));
        break;
    case EmitterShape::Sphere: {
        Vector3 dir(randRange(rng, -1.f, 1.f), randRange(rng, -1.f, 1.f), randRange(rng, -1.f, 1.f));
        if (dir.LengthSquared() < 1e-8f) dir = Vector3(0.f, 1.f, 0.f);
        dir.Normalize();
        float r = shapeRadius * std::cbrt(randRange(rng, 0.f, 1.f));
        localOffset = dir * r;
        break;
    }
    case EmitterShape::Cone: {
        // Random point on the cone's circular base (XZ plane around the local origin).
        float r   = shapeRadius * std::sqrt(randRange(rng, 0.f, 1.f));
        float phi = randRange(rng, 0.f, 2.f * kPi);
        localOffset = Vector3(r * std::cos(phi), 0.f, r * std::sin(phi));
        break;
    }
    case EmitterShape::Point:
    default:
        break;
    }

    if (!owner) return localOffset;
    const Matrix& world = owner->getTransform()->getGlobalMatrix();
    return Vector3::Transform(localOffset, world);
}

void ComponentParticleSystem::spawnParticle() {
    Particle* slot = nullptr;
    if ((int)m_particles.size() < maxParticles) {
        m_particles.emplace_back();
        slot = &m_particles.back();
    } else {
        // Recycle the oldest dead slot, or — failing that — the closest-to-death particle.
        for (auto& p : m_particles) if (!p.alive) { slot = &p; break; }
        if (!slot) {
            slot = &m_particles.front();
            for (auto& p : m_particles)
                if (p.lifetime - p.age < slot->lifetime - slot->age) slot = &p;
        }
    }

    Particle& p   = *slot;
    p.alive       = true;
    p.age         = 0.f;
    p.lifetime    = std::max(0.01f, randRange(m_rng, lifeRange.x, lifeRange.y));
    p.position    = randomEmitPosition(m_rng);
    p.velocity    = randomEmitDirection(m_rng) * randRange(m_rng, speedRange.x, speedRange.y);
    p.rotationDeg = randRange(m_rng, rotationRange.x, rotationRange.y);
    p.baseSize    = std::max(0.001f, randRange(m_rng, sizeRange.x, sizeRange.y));

    const int totalTiles = std::max(1, sheetColumns * sheetRows);
    p.frameIndex = randomFrame ? (int)(randRange(m_rng, 0.f, (float)totalTiles - 1e-3f)) : 0;
}

void ComponentParticleSystem::update(float dt) {
    if (!enabled) return;

    if (playing) {
        m_age += dt;
        if (looping || m_age <= duration) {
            m_spawnAccumulator += emissionRate * dt;
            while (m_spawnAccumulator >= 1.f && (int)std::count_if(m_particles.begin(), m_particles.end(),
                                                                    [](const Particle& p) { return p.alive; }) < maxParticles) {
                spawnParticle();
                m_spawnAccumulator -= 1.f;
            }
        }
    }

    for (auto& p : m_particles) {
        if (!p.alive) continue;
        p.age += dt;
        if (p.age >= p.lifetime) { p.alive = false; continue; }

        p.velocity += gravity * dt;
        p.position += p.velocity * dt;
    }
}

void ComponentParticleSystem::onEditor() {
    ImGui::Checkbox("Enabled##ps", &enabled);
    ImGui::SameLine();
    if (ImGui::Checkbox("Playing##ps", &playing)) {}
    ImGui::SameLine();
    if (ImGui::Button("Clear##ps")) clear();

    ImGui::Separator();
    ImGui::TextUnformatted("Emitter");
    ImGui::Checkbox("Looping", &looping);
    if (!looping) ImGui::DragFloat("Duration", &duration, 0.1f, 0.01f, 120.f);
    ImGui::DragFloat("Emission rate (per sec)", &emissionRate, 0.5f, 0.f, 10000.f);
    ImGui::DragInt("Max particles", &maxParticles, 1.f, 1, 100000);
    ImGui::Checkbox("World space", &worldSpace);

    static const char* kShapes[] = { "Point", "Box", "Sphere", "Cone" };
    int shapeIdx = (int)shape;
    if (ImGui::Combo("Shape", &shapeIdx, kShapes, IM_ARRAYSIZE(kShapes)))
        shape = (EmitterShape)shapeIdx;

    if (shape == EmitterShape::Box || shape == EmitterShape::Sphere || shape == EmitterShape::Cone)
        ImGui::DragFloat("Shape radius", &shapeRadius, 0.01f, 0.f, 1000.f);
    if (shape == EmitterShape::Cone)
        ImGui::DragFloat("Cone angle (deg)", &coneAngleDeg, 0.5f, 0.f, 89.f);

    ImGui::Separator();
    ImGui::TextUnformatted("Initial values (random range)");
    ImGui::DragFloat2("Lifetime", &lifeRange.x, 0.05f, 0.01f, 120.f);
    ImGui::DragFloat2("Speed", &speedRange.x, 0.05f, 0.f, 1000.f);
    ImGui::DragFloat2("Size", &sizeRange.x, 0.01f, 0.001f, 1000.f);
    ImGui::DragFloat2("Rotation (deg)", &rotationRange.x, 0.5f, -360.f, 360.f);
    ImGui::DragFloat3("Gravity", &gravity.x, 0.05f, -100.f, 100.f);

    ImGui::Separator();
    ImGui::TextUnformatted("Over lifetime");
    ImGui::ColorEdit4("Start colour", &startColor.x);
    ImGui::ColorEdit4("End colour", &endColor.x);
    ImGui::DragFloat("Start size mult.", &startSizeMul, 0.01f, 0.f, 100.f);
    ImGui::DragFloat("End size mult.", &endSizeMul, 0.01f, 0.f, 100.f);

    ImGui::Separator();
    ImGui::TextUnformatted("Render");
    {
        ImGui::TextUnformatted("Texture");
        ImGui::SameLine(90.f);
        char buf[256];
        strncpy_s(buf, texturePath.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("##psTexture", buf, sizeof(buf)))
            texturePath = buf;
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload(kDragAsset)) {
                texturePath.assign(static_cast<const char*>(pl->Data), pl->DataSize - 1);
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::DragInt("Sheet columns", &sheetColumns, 1.f, 1, 64);
    ImGui::DragInt("Sheet rows", &sheetRows, 1.f, 1, 64);
    ImGui::Checkbox("Random sub-image per particle", &randomFrame);

    static const char* kBlend[] = { "Alpha", "Additive" };
    int blendIdx = (int)blendMode;
    if (ImGui::Combo("Blend mode", &blendIdx, kBlend, IM_ARRAYSIZE(kBlend)))
        blendMode = (BlendMode)blendIdx;

    ImGui::DragInt("Layer", &layer, 1.f, -100, 100);

    ImGui::Separator();
    int alive = (int)std::count_if(m_particles.begin(), m_particles.end(), [](const Particle& p) { return p.alive; });
    ImGui::Text("Live particles: %d / %d", alive, maxParticles);
}

void ComponentParticleSystem::onSave(std::string& outJson) const {
    outJson += "\"enabled\":" + std::string(enabled ? "true" : "false") + ",";
    outJson += "\"playing\":" + std::string(playing ? "true" : "false") + ",";
    outJson += "\"looping\":" + std::string(looping ? "true" : "false") + ",";
    outJson += "\"duration\":" + std::to_string(duration) + ",";
    outJson += "\"emissionRate\":" + std::to_string(emissionRate) + ",";
    outJson += "\"maxParticles\":" + std::to_string(maxParticles) + ",";
    outJson += "\"shape\":" + std::to_string((int)shape) + ",";
    outJson += "\"shapeRadius\":" + std::to_string(shapeRadius) + ",";
    outJson += "\"coneAngleDeg\":" + std::to_string(coneAngleDeg) + ",";
    outJson += "\"worldSpace\":" + std::string(worldSpace ? "true" : "false") + ",";
    outJson += "\"lifeRange\":[" + std::to_string(lifeRange.x) + "," + std::to_string(lifeRange.y) + "],";
    outJson += "\"speedRange\":[" + std::to_string(speedRange.x) + "," + std::to_string(speedRange.y) + "],";
    outJson += "\"sizeRange\":[" + std::to_string(sizeRange.x) + "," + std::to_string(sizeRange.y) + "],";
    outJson += "\"rotationRange\":[" + std::to_string(rotationRange.x) + "," + std::to_string(rotationRange.y) + "],";
    outJson += "\"startColor\":[" + std::to_string(startColor.x) + "," + std::to_string(startColor.y) + "," +
                                     std::to_string(startColor.z) + "," + std::to_string(startColor.w) + "],";
    outJson += "\"endColor\":[" + std::to_string(endColor.x) + "," + std::to_string(endColor.y) + "," +
                                   std::to_string(endColor.z) + "," + std::to_string(endColor.w) + "],";
    outJson += "\"startSizeMul\":" + std::to_string(startSizeMul) + ",";
    outJson += "\"endSizeMul\":" + std::to_string(endSizeMul) + ",";
    outJson += "\"gravity\":[" + std::to_string(gravity.x) + "," + std::to_string(gravity.y) + "," + std::to_string(gravity.z) + "],";
    outJson += "\"texturePath\":\"" + texturePath + "\",";
    outJson += "\"sheetColumns\":" + std::to_string(sheetColumns) + ",";
    outJson += "\"sheetRows\":" + std::to_string(sheetRows) + ",";
    outJson += "\"randomFrame\":" + std::string(randomFrame ? "true" : "false") + ",";
    outJson += "\"blendMode\":" + std::to_string((int)blendMode) + ",";
    outJson += "\"layer\":" + std::to_string(layer);
}

void ComponentParticleSystem::onLoad(const std::string& json) {
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

    auto getBool  = [&](const char* key, bool def) { auto v = extract(key); return v.empty() ? def : (v == "true"); };
    auto getFloat = [&](const char* key, float def) { auto v = extract(key); return v.empty() ? def : std::stof(v); };
    auto getInt   = [&](const char* key, int def)   { auto v = extract(key); return v.empty() ? def : std::stoi(v); };

    enabled      = getBool("enabled", true);
    playing      = getBool("playing", true);
    looping      = getBool("looping", true);
    duration     = getFloat("duration", duration);
    emissionRate = getFloat("emissionRate", emissionRate);
    maxParticles = getInt("maxParticles", maxParticles);
    shape        = (EmitterShape)getInt("shape", (int)shape);
    shapeRadius  = getFloat("shapeRadius", shapeRadius);
    coneAngleDeg = getFloat("coneAngleDeg", coneAngleDeg);
    worldSpace   = getBool("worldSpace", worldSpace);

    if (auto v = extract("lifeRange");     !v.empty()) extractArray(v, &lifeRange.x, 2);
    if (auto v = extract("speedRange");    !v.empty()) extractArray(v, &speedRange.x, 2);
    if (auto v = extract("sizeRange");     !v.empty()) extractArray(v, &sizeRange.x, 2);
    if (auto v = extract("rotationRange"); !v.empty()) extractArray(v, &rotationRange.x, 2);
    if (auto v = extract("startColor");    !v.empty()) extractArray(v, &startColor.x, 4);
    if (auto v = extract("endColor");      !v.empty()) extractArray(v, &endColor.x, 4);
    if (auto v = extract("gravity");       !v.empty()) extractArray(v, &gravity.x, 3);

    startSizeMul = getFloat("startSizeMul", startSizeMul);
    endSizeMul   = getFloat("endSizeMul", endSizeMul);
    texturePath  = extract("texturePath");
    sheetColumns = getInt("sheetColumns", sheetColumns);
    sheetRows    = getInt("sheetRows", sheetRows);
    randomFrame  = getBool("randomFrame", randomFrame);
    blendMode    = (BlendMode)getInt("blendMode", (int)blendMode);
    layer        = getInt("layer", layer);
}
