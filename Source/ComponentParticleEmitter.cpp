#include "Globals.h"
#include "ComponentParticleEmitter.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include <imgui.h>

ComponentParticleEmitter::ComponentParticleEmitter(GameObject* o) : Component(o) {}

EmitterDesc ComponentParticleEmitter::buildDesc() const {
    EmitterDesc d;
    d.position    = owner->getTransform()->getGlobalMatrix().Translation();
    d.velocity    = velocity;
    d.velocityVar = velocityVar;
    d.gravity     = gravity;
    d.colourStart = colourStart;
    d.colourEnd   = colourEnd;
    d.lifetime    = lifetime;
    d.lifetimeVar = lifetimeVar;
    d.halfSize    = halfSize;
    d.spawnPerSec = spawnPerSec;
    return d;
}

void ComponentParticleEmitter::onEditor() {
    ImGui::Checkbox("Enabled##pe", &enabled);
    ImGui::DragInt("Spawn / sec", &spawnPerSec, 1, 0, 2000);
    ImGui::DragFloat3("Velocity",    &velocity.x,    0.05f);
    ImGui::DragFloat3("Vel Spread",  &velocityVar.x, 0.05f, 0.f, 20.f);
    ImGui::DragFloat3("Gravity",     &gravity.x,     0.1f);
    ImGui::ColorEdit4("Start Colour",&colourStart.x);
    ImGui::ColorEdit4("End Colour",  &colourEnd.x);
    ImGui::DragFloat("Lifetime",     &lifetime,    0.05f, 0.1f, 30.f);
    ImGui::DragFloat("Life Spread",  &lifetimeVar, 0.05f, 0.f,  10.f);
    ImGui::DragFloat("Half Size",    &halfSize,    0.005f, 0.001f, 2.f);
}

void ComponentParticleEmitter::onSave(std::string& j) const {
    j += "\"enabled\":"    + std::string(enabled ? "true" : "false") + ",";
    j += "\"spawnPerSec\":" + std::to_string(spawnPerSec) + ",";
    j += "\"lifetime\":"   + std::to_string(lifetime) + ",";
    j += "\"halfSize\":"   + std::to_string(halfSize);
}

void ComponentParticleEmitter::onLoad(const std::string& json) {
    auto get = [&](const char* key) -> std::string {
        std::string k = "\"" + std::string(key) + "\":";
        auto pos = json.find(k);
        if (pos == std::string::npos) return {};
        pos += k.size();
        auto end = json.find_first_of(",}", pos);
        return json.substr(pos, end - pos);
    };
    auto s = get("spawnPerSec"); if (!s.empty()) spawnPerSec = std::stoi(s);
    auto l = get("lifetime");    if (!l.empty()) lifetime    = std::stof(l);
    auto h = get("halfSize");    if (!h.empty()) halfSize    = std::stof(h);
    auto e = get("enabled");     if (!e.empty()) enabled     = (e == "true");
}
