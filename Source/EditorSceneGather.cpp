#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include <ole2.h>
#include "DragDropManager.h"
#include "EngineDropTarget.h"
#include "ModuleD3D12.h"
#include "ComponentAnimation.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ImGuiPass.h"
#include "DebugDrawPass.h"
#include "ForwardMeshPass.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "DecalPass.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
#include "RenderTexture.h"
#include "EmptyScene.h"
#include "SceneGraph.h"
#include "SceneManager.h"
#include "ShaderTableDesc.h"
#include "MeshPipeline.h"
#include "FileDialog.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentFactory.h"
#include "PrimitiveFactory.h"
#include "ModuleCamera.h"
#include "FrustumDebugDraw.h"
#include "BoundingVolume.h"
#include "ComponentBounds.h"
#include "ComponentRigidbody.h"
#include "CollisionSystem.h"
#include "CollisionResponse.h"
#include "EnvironmentMap.h"
#include "SceneViewPanel.h"
#include "GameViewPanel.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "AssetBrowserPanel.h"
#include "SceneSettingsPanel.h"
#include "PrefabManager.h"
#include "FileWatcher.h"
#include "ResourceMaterial.h"
#include "ResourceModel.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "Model.h"
#include "Mesh.h"
#include "MeshEntry.h"
#include "ResourceMesh.h"
#include <d3dx12.h>
#include "ModuleStaticBuffer.h"
#include <filesystem>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <cfloat>

static constexpr float kDeg2Rad = 0.0174532925f;

void ModuleEditor::gatherLights(GameObject* node, FrameLightData& out) const{
    if (!node || !node->isActive()) return;

    if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled){
        if (out.dirLights.size() < MeshPipeline::MAX_DIR_LIGHTS){
            MeshPipeline::GPUDirectionalLight g;
            g.direction = dl->direction;
            g.direction.Normalize();
            g.color = dl->color;
            g.intensity = dl->intensity;
            g._pad = 0.f;
            out.dirLights.push_back(g);
        }
    }

    if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled){
        if (out.pointLights.size() < MeshPipeline::MAX_POINT_LIGHTS){
            MeshPipeline::GPUPointLight p;
            p.position = node->getTransform()->getGlobalMatrix().Translation();
            p.squaredRadius = pl->radius * pl->radius;
            p.color = pl->color;
            p.intensity = pl->intensity;
            out.pointLights.push_back(p);
        }
    }

    if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled){
        if (out.spotLights.size() < MeshPipeline::MAX_SPOT_LIGHTS){
            MeshPipeline::GPUSpotLight s;
            s.position = node->getTransform()->getGlobalMatrix().Translation();
            s.direction = sl->direction;
            s.direction.Normalize();
            s.squaredRadius = sl->radius * sl->radius;
            s.innerAngle = cosf(sl->innerAngle * kDeg2Rad);
            s.outerAngle = cosf(sl->outerAngle * kDeg2Rad);
            s.color = sl->color;
            s.intensity = sl->intensity;
            s._pad[0] = s._pad[1] = s._pad[2] = 0.f;
            out.spotLights.push_back(s);
        }
    }

    for (auto* c : node->getChildren()) gatherLights(c, out);
}

void ModuleEditor::gatherDecals(GameObject* node, std::vector<DecalInstance>& out,
                                  const Matrix& view, const Matrix& proj,
                                  uint32_t w, uint32_t h) const{
    if (!node || !node->isActive()) return;

    if (auto* dc = node->getComponent<ComponentDecal>(); dc && dc->enabled){
        if (out.size() < DecalPass::MAX_DECALS){
            Matrix worldMat = node->getTransform()->getGlobalMatrix();
            Matrix viewProj = view * proj;

            DecalInstance inst;
            inst.mvp = (worldMat * viewProj).Transpose();
            worldMat.Invert(inst.invModel);
            inst.invModel = inst.invModel.Transpose();

            Matrix invVP;
            viewProj.Invert(invVP);
            inst.invViewProj = invVP.Transpose();

            inst.colourOpacity = Vector4(dc->colour.x, dc->colour.y, dc->colour.z, dc->opacity);

            out.push_back(inst);
        }
    }

    for (auto* c : node->getChildren()) gatherDecals(c, out, view, proj, w, h);
}

void ModuleEditor::gatherBillboards(GameObject* node, std::vector<BillboardInstance>& out,
                                     const Matrix& view, const Matrix& viewProj,
                                     const Vector3& camPos, const Vector3& camRight, const Vector3& camUp) const{
    if (!node || !node->isActive()) return;

    if (auto* bb = node->getComponent<ComponentBillboard>(); bb && bb->enabled){
        if (out.size() < BillboardPass::MAX_BILLBOARDS){
            const Vector3 center = node->getTransform()->getGlobalMatrix().Translation();

            Vector3 right, up;
            switch (bb->alignment){
            case ComponentBillboard::Alignment::Screen:
                right = camRight;
                up = camUp;
                break;
            case ComponentBillboard::Alignment::World: {
                Vector3 worldUp(0.f, 1.f, 0.f);
                Vector3 n = camPos - center;
                if (n.LengthSquared() < 1e-8f) n = -camRight;
                n.Normalize();
                right = worldUp.Cross(n);
                if (right.LengthSquared() < 1e-8f) right = camRight;
                right.Normalize();
                up = n.Cross(right);
                break;
            }
            case ComponentBillboard::Alignment::Axial:
            default: {
                Vector3 fixedUp(0.f, 1.f, 0.f);
                Vector3 toCam = camPos - center;
                right = toCam.Cross(fixedUp);
                if (right.LengthSquared() < 1e-8f) right = camRight;
                right.Normalize();
                up = fixedUp;
                break;
            }
            }

            const int cols = std::max(1, bb->sheetColumns);
            const int rows = std::max(1, bb->sheetRows);
            const int totalTiles = cols * rows;
            const float frame = bb->getCurrentFrame();
            const int frameA = ((int)frame) % totalTiles;
            const int frameB = (frameA + 1) % totalTiles;
            const float blend = frame - floorf(frame);

            auto tileRect = [cols, rows](int tileIndex) -> Vector4 {
                int tx = tileIndex % cols;
                int ty = tileIndex / cols;
                ty = (rows - 1) - ty;
                float u0 = (float)tx / (float)cols;
                float v0 = (float)ty / (float)rows;
                return Vector4(u0, v0, u0 + 1.f / cols, v0 + 1.f / rows);
            };

            BillboardInstance inst;
            inst.cb.viewProj = viewProj.Transpose();
            inst.cb.centerHalfWidth = Vector4(center.x, center.y, center.z, bb->size.x * 0.5f);
            inst.cb.rightHalfHeight = Vector4(right.x, right.y, right.z, bb->size.y * 0.5f);
            inst.cb.up = Vector4(up.x, up.y, up.z, 0.f);
            inst.cb.tint = bb->tint;
            inst.cb.frameRectA = tileRect(frameA);
            inst.cb.frameRectB = (totalTiles > 1) ? tileRect(frameB) : inst.cb.frameRectA;
            inst.cb.blendFactor = Vector4(blend, 0.f, 0.f, 0.f);
            inst.texturePath = bb->texturePath;

            out.push_back(std::move(inst));
        }
    }

    for (auto* c : node->getChildren()) gatherBillboards(c, out, view, viewProj, camPos, camRight, camUp);
}

void ModuleEditor::gatherParticleSystems(GameObject* node, std::vector<BillboardInstance>& out,
                                          const Matrix& viewProj,
                                          const Vector3& camPos, const Vector3& camRight, const Vector3& camUp) const{
    if (!node || !node->isActive()) return;

    if (auto* ps = node->getComponent<ComponentParticleSystem>();
        ps && ps->enabled && !ps->useGPU){
        const int cols = std::max(1, ps->sheetColumns);
        const int rows = std::max(1, ps->sheetRows);
        const int totalTiles = cols * rows;

        auto tileRect = [cols, rows](int tileIndex) -> Vector4 {
            int tx = tileIndex % cols;
            int ty = tileIndex / cols;
            ty = (rows - 1) - ty;
            float u0 = (float)tx / (float)cols;
            float v0 = (float)ty / (float)rows;
            return Vector4(u0, v0, u0 + 1.f / cols, v0 + 1.f / rows);
        };

        for (const auto& p : ps->getParticles()){
            if (!p.alive) continue;
            if (out.size() >= BillboardPass::MAX_BILLBOARDS) break;

            const float t = std::clamp(p.age / std::max(0.0001f, p.lifetime), 0.f, 1.f);
            const float size = p.baseSize * ps->sizeMultiplierAt(t);
            const Vector4 color = ps->colorAt(t);

            const float rad = p.rotationDeg * (3.14159265358979323846f / 180.f);
            const float cs = std::cos(rad), sn = std::sin(rad);
            const Vector3 right = camRight * cs + camUp * sn;
            const Vector3 up = camUp * cs - camRight * sn;

            const Vector4 frameA = tileRect(p.frameIndex % totalTiles);

            BillboardInstance inst;
            inst.cb.viewProj = viewProj.Transpose();
            inst.cb.centerHalfWidth = Vector4(p.position.x, p.position.y, p.position.z, size * 0.5f);
            inst.cb.rightHalfHeight = Vector4(right.x, right.y, right.z, size * 0.5f);
            inst.cb.up = Vector4(up.x, up.y, up.z, 0.f);
            inst.cb.tint = color;
            inst.cb.frameRectA = frameA;
            inst.cb.frameRectB = frameA;
            inst.cb.blendFactor = Vector4(0.f, 0.f, 0.f, 0.f);
            inst.texturePath = ps->texturePath;
            inst.additive = (ps->blendMode == ComponentParticleSystem::BlendMode::Additive);

            out.push_back(std::move(inst));
        }
    }

    for (auto* c : node->getChildren()) gatherParticleSystems(c, out, viewProj, camPos, camRight, camUp);
}

void ModuleEditor::gatherTrails(GameObject* node, std::vector<TrailInstance>& out,
                                const Matrix& viewProj, const Vector3& camPos) const{
    if (!node || !node->isActive()) return;

    if (auto* tr = node->getComponent<ComponentTrail>(); tr && tr->enabled){
        if (out.size() < TrailPass::MAX_TRAILS){
            TrailInstance inst;
            bool built = tr->buildMesh(camPos, inst.vertices);
            if (built && !inst.vertices.empty()){
                inst.tint = Vector4(1.f, 1.f, 1.f, 1.f);
                inst.texturePath = tr->texturePath;
                inst.additive = (tr->blendMode == ComponentTrail::BlendMode::Additive);
                inst.sortPos = inst.vertices.front().position;
                inst.layer = tr->layer;
                out.push_back(std::move(inst));
            }
        }
    }

    for (auto* c : node->getChildren()) gatherTrails(c, out, viewProj, camPos);
}

void ModuleEditor::gatherGPUParticles(GameObject* node,
                                       std::vector<ParticleDrawRequest>& out,
                                       const Vector3& ,
                                       const Vector3& , const Vector3& ,
                                       float elapsedTime) const {
    if (!node || !node->isActive()) return;

    if (auto* ps = node->getComponent<ComponentParticleSystem>();
        ps && ps->enabled && ps->useGPU){
        const int cols = std::max(1, ps->sheetColumns);
        const int rows = std::max(1, ps->sheetRows);
        const int totalTiles = cols * rows;

        auto tileUV = [cols, rows](int tileIdx) -> std::pair<Vector2, Vector2> {
            int tx = tileIdx % cols;
            int ty = tileIdx / cols;
            ty = (rows - 1) - ty;
            float u0 = (float)tx / cols, u1 = u0 + 1.f / cols;
            float v0 = (float)ty / rows, v1 = v0 + 1.f / rows;
            return { Vector2(u0, v0), Vector2(u1, v1) };
        };

        ParticleDrawRequest req;
        req.emitterKey = reinterpret_cast<size_t>(ps);
        req.maxParticles = ps->maxParticles;
        req.texturePath = ps->texturePath;
        req.additive = (ps->blendMode == ComponentParticleSystem::BlendMode::Additive);
        req.gpuTurbulence = ps->useTurbulence;
        req.turbFrequency = ps->turbulenceFrequency;
        req.turbStrength = ps->turbulenceStrength;
        req.turbScrollSpeed = ps->turbulenceScroll;
        req.time = elapsedTime;
        req.deltaTime = std::clamp((float)app->getElapsedMilis() * 0.001f, 0.f, 0.1f);

        for (const auto& p : ps->getParticles()){
            if (!p.alive) continue;
            if ((int)req.particles.size() >= ps->maxParticles) break;

            const float t = std::clamp(p.age / std::max(0.0001f, p.lifetime), 0.f, 1.f);
            const float size = p.baseSize * ps->sizeMultiplierAt(t);
            const Vector4 col = ps->colorAt(t);

            auto [uvMin, uvMax] = tileUV(p.frameIndex % totalTiles);

            GpuParticle gp;
            gp.position[0] = p.position.x;
            gp.position[1] = p.position.y;
            gp.position[2] = p.position.z;
            gp.size = size;
            gp.color[0] = col.x;
            gp.color[1] = col.y;
            gp.color[2] = col.z;
            gp.color[3] = col.w;
            gp.rotation = p.rotationDeg;
            gp.uvMin[0] = uvMin.x;
            gp.uvMin[1] = uvMin.y;
            gp.uvMax[0] = uvMax.x;
            gp.uvMax[1] = uvMax.y;
            req.particles.push_back(gp);
        }

        if (!req.particles.empty())
            out.push_back(std::move(req));
    }

    for (auto* c : node->getChildren())
        gatherGPUParticles(c, out, {}, {}, {}, elapsedTime);
}

void ModuleEditor::debugDrawLights(SceneGraph* scene, float sz){
    if (!scene) return;
    auto v = [](const Vector3& x) -> const float* { return &x.x; };
    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (!node || !node->isActive()) return;
        if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled){
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            Vector3 d = dl->direction; d.Normalize();
            float h = sz * .2f;
            dd::line(v(p), v(p + d * sz * 2.f), dd::colors::Yellow);
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Yellow);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Yellow);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Yellow);
        }
        if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled){
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            float h = sz * .2f;
            dd::sphere(v(p), dd::colors::Cyan, pl->radius);
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Cyan);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Cyan);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Cyan);
        }
        if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled){
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            Vector3 dir = sl->direction; dir.Normalize();
            float outerR = tanf(sl->outerAngle * kDeg2Rad) * sl->radius;
            Vector3 tip = p + dir * sl->radius;
            Vector3 up = (fabsf(dir.y) < .99f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
            Vector3 right = dir.Cross(up); right.Normalize();
            up = right.Cross(dir); up.Normalize();
            const int segs = 8;
            for (int i = 0; i < segs; ++i){
                float a0 = float(i) / segs * 6.28318530f;
                float a1 = float(i + 1) / segs * 6.28318530f;
                Vector3 o0 = tip + (right * cosf(a0) + up * sinf(a0)) * outerR;
                Vector3 o1 = tip + (right * cosf(a1) + up * sinf(a1)) * outerR;
                dd::line(v(p), v(o0), dd::colors::Orange);
                dd::line(v(o0), v(o1), dd::colors::Orange);
            }
            float h = sz * .2f;
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Orange);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Orange);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Orange);
        }
        for (auto* c : node->getChildren()) visit(c);
        };
    visit(scene->getRoot());
}
