#include "Globals.h"
#include "PrimitiveFactory.h"
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include "ComponentMesh.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include <cmath>
#include <vector>


static constexpr float kPI = 3.14159265358979323846f;
static constexpr float kPI2 = kPI * 2.f;

static void pushQuad(std::vector<uint32_t>& idx, uint32_t a, uint32_t b, uint32_t c, uint32_t d){
    idx.push_back(a); idx.push_back(b); idx.push_back(c);
    idx.push_back(a); idx.push_back(c); idx.push_back(d);
}


std::unique_ptr<Mesh> PrimitiveFactory::createQuadMesh(){
    std::vector<Mesh::Vertex> verts = {
        { Vector3(-0.5f,-0.5f,0.f), Vector2(0.f,1.f), Vector3(0,0,1), Vector4(1,0,0,1) },
        { Vector3(-0.5f, 0.5f,0.f), Vector2(0.f,0.f), Vector3(0,0,1), Vector4(1,0,0,1) },
        { Vector3( 0.5f, 0.5f,0.f), Vector2(1.f,0.f), Vector3(0,0,1), Vector4(1,0,0,1) },
        { Vector3( 0.5f,-0.5f,0.f), Vector2(1.f,1.f), Vector3(0,0,1), Vector4(1,0,0,1) },
    };
    std::vector<uint32_t> indices = { 0,1,2, 0,2,3 };
    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, indices, 0);
    return mesh;
}

std::unique_ptr<Model> PrimitiveFactory::createQuadModel(std::unique_ptr<Material> material){
    auto model = std::make_unique<Model>();
    model->addMesh(createQuadMesh());
    model->addMaterial(std::move(material));
    return model;
}

std::unique_ptr<Model> PrimitiveFactory::createTexturedQuad(ComPtr<ID3D12Resource> texture,
                                                              D3D12_GPU_DESCRIPTOR_HANDLE srv){
    auto mat = std::make_unique<Material>();
    mat->setBaseColorTexture(texture, srv);
    return createQuadModel(std::move(mat));
}

GameObject* PrimitiveFactory::createTexturedQuadObject(SceneGraph* scene, const std::string& name,
                                                         ComPtr<ID3D12Resource> texture,
                                                         D3D12_GPU_DESCRIPTOR_HANDLE srv){
    GameObject* go = scene->createGameObject(name);
    go->createComponent<ComponentMesh>()->setProceduralModel(createTexturedQuad(texture, srv));
    return go;
}


std::unique_ptr<Model> PrimitiveFactory::meshToModel(std::unique_ptr<Mesh> mesh){
    auto model = std::make_unique<Model>();
    model->addMesh(std::move(mesh));
    model->addMaterial(std::make_unique<Material>());
    return model;
}


std::unique_ptr<Mesh> PrimitiveFactory::createCubeMesh(){
    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> idx;
    verts.reserve(24);
    idx.reserve(36);

    auto addFace = [&](Vector3 bl, Vector3 tl, Vector3 tr, Vector3 br,
                       Vector3 n, Vector3 t){
        uint32_t base = static_cast<uint32_t>(verts.size());
        Vector4 tan(t.x, t.y, t.z, 1.f);
        verts.push_back({ bl, {0,1}, n, tan });
        verts.push_back({ tl, {0,0}, n, tan });
        verts.push_back({ tr, {1,0}, n, tan });
        verts.push_back({ br, {1,1}, n, tan });
        pushQuad(idx, base, base+1, base+2, base+3);
    };

    const float h = 0.5f;

    addFace({h,-h,+h},{h,+h,+h},{h,+h,-h},{h,-h,-h}, {1,0,0},{0,0,-1});
    addFace({-h,-h,-h},{-h,+h,-h},{-h,+h,+h},{-h,-h,+h}, {-1,0,0},{0,0,1});
    addFace({-h,h,+h},{-h,h,-h},{+h,h,-h},{+h,h,+h}, {0,1,0},{1,0,0});
    addFace({-h,-h,-h},{-h,-h,+h},{+h,-h,+h},{+h,-h,-h}, {0,-1,0},{1,0,0});
    addFace({-h,-h,h},{-h,+h,h},{+h,+h,h},{+h,-h,h}, {0,0,1},{1,0,0});
    addFace({+h,-h,-h},{+h,+h,-h},{-h,+h,-h},{-h,-h,-h}, {0,0,-1},{-1,0,0});

    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, idx, 0);
    return mesh;
}


std::unique_ptr<Mesh> PrimitiveFactory::createSphereMesh(int rings, int segments){
    rings = std::max(rings, 3);
    segments = std::max(segments, 3);

    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> idx;

    for (int r = 0; r <= rings; ++r){
        float lat = static_cast<float>(r) * kPI / rings;
        float cosL = cosf(lat), sinL = sinf(lat);
        float y = cosL * 0.5f;
        float xzr = sinL * 0.5f;

        for (int s = 0; s <= segments; ++s){
            float lon = static_cast<float>(s) * kPI2 / segments;
            float sinS = sinf(lon), cosS = cosf(lon);

            Vector3 pos(xzr * sinS, y, xzr * cosS);
            Vector3 nrm(sinL * sinS, cosL, sinL * cosS);
            Vector2 uv(static_cast<float>(s) / segments, static_cast<float>(r) / rings);
            Vector4 tan(cosS, 0.f, -sinS, 1.f);

            verts.push_back({ pos, uv, nrm, tan });
        }
    }

    int cols = segments + 1;
    for (int r = 0; r < rings; ++r){
        for (int s = 0; s < segments; ++s){
            uint32_t tl = r * cols + s;
            uint32_t tr = tl + 1;
            uint32_t bl = tl + cols;
            uint32_t br = bl + 1;
            pushQuad(idx, tl, bl, br, tr);
        }
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, idx, 0);
    return mesh;
}


std::unique_ptr<Mesh> PrimitiveFactory::createCapsuleMesh(int halfRings, int segments){
    halfRings = std::max(halfRings, 2);
    segments = std::max(segments, 4);

    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> idx;

    int totalRows = 2 * halfRings + 1;
    int cols = segments + 1;

    for (int r = 0; r <= totalRows; ++r){
        float lat, y;
        if (r <= halfRings){
            lat = static_cast<float>(r) * kPI * 0.5f / halfRings;
            y = 0.5f + 0.5f * cosf(lat);
        } else {
            int r2 = r - halfRings;
            lat = kPI * 0.5f + static_cast<float>(r2) * kPI * 0.5f / halfRings;
            y = -0.5f + 0.5f * cosf(lat);
        }
        float sinL = sinf(lat), cosL = cosf(lat);
        float xzr = 0.5f * sinL;

        for (int s = 0; s <= segments; ++s){
            float lon = static_cast<float>(s) * kPI2 / segments;
            float sinS = sinf(lon), cosS = cosf(lon);

            Vector3 pos(xzr * sinS, y, xzr * cosS);
            Vector3 nrm(sinL * sinS, cosL, sinL * cosS);
            Vector2 uv(static_cast<float>(s) / segments, static_cast<float>(r) / totalRows);
            Vector4 tan(cosS, 0.f, -sinS, 1.f);

            verts.push_back({ pos, uv, nrm, tan });
        }
    }

    for (int r = 0; r < totalRows; ++r){
        for (int s = 0; s < segments; ++s){
            uint32_t tl = r * cols + s;
            uint32_t tr = tl + 1;
            uint32_t bl = tl + cols;
            uint32_t br = bl + 1;
            pushQuad(idx, tl, bl, br, tr);
        }
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, idx, 0);
    return mesh;
}


std::unique_ptr<Mesh> PrimitiveFactory::createPlaneMesh(){
    const float h = 0.5f;
    std::vector<Mesh::Vertex> verts = {
        { {-h, 0, +h}, {0,1}, {0,1,0}, {1,0,0,1} },
        { {-h, 0, -h}, {0,0}, {0,1,0}, {1,0,0,1} },
        { {+h, 0, -h}, {1,0}, {0,1,0}, {1,0,0,1} },
        { {+h, 0, +h}, {1,1}, {0,1,0}, {1,0,0,1} },
    };
    std::vector<uint32_t> idx;
    pushQuad(idx, 0, 1, 2, 3);

    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, idx, 0);
    return mesh;
}


std::unique_ptr<Mesh> PrimitiveFactory::createCylinderMesh(int segments){
    segments = std::max(segments, 4);

    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> idx;

    const float r = 0.5f, halfH = 0.5f;

    for (int s = 0; s <= segments; ++s){
        float lon = static_cast<float>(s) * kPI2 / segments;
        float sinS = sinf(lon), cosS = cosf(lon);
        Vector3 nrm(sinS, 0.f, cosS);
        Vector4 tan(cosS, 0.f, -sinS, 1.f);
        float u = static_cast<float>(s) / segments;

        verts.push_back({ {r*sinS, +halfH, r*cosS}, {u, 0}, nrm, tan });
        verts.push_back({ {r*sinS, -halfH, r*cosS}, {u, 1}, nrm, tan });
    }
    for (int s = 0; s < segments; ++s){
        uint32_t t0 = s * 2, t1 = (s+1) * 2;
        uint32_t b0 = t0 + 1, b1 = t1 + 1;
        pushQuad(idx, t0, b0, b1, t1);
    }

    uint32_t topCentre = static_cast<uint32_t>(verts.size());
    verts.push_back({ {0, +halfH, 0}, {0.5f,0.5f}, {0,1,0}, {1,0,0,1} });
    uint32_t topRingBase = static_cast<uint32_t>(verts.size());
    for (int s = 0; s <= segments; ++s){
        float lon = static_cast<float>(s) * kPI2 / segments;
        float sinS = sinf(lon), cosS = cosf(lon);
        Vector2 uv(0.5f + 0.5f*sinS, 0.5f - 0.5f*cosS);
        verts.push_back({ {r*sinS, +halfH, r*cosS}, uv, {0,1,0}, {1,0,0,1} });
    }
    for (int s = 0; s < segments; ++s){
        idx.push_back(topCentre);
        idx.push_back(topRingBase + s + 1);
        idx.push_back(topRingBase + s);
    }

    uint32_t botCentre = static_cast<uint32_t>(verts.size());
    verts.push_back({ {0, -halfH, 0}, {0.5f,0.5f}, {0,-1,0}, {1,0,0,1} });
    uint32_t botRingBase = static_cast<uint32_t>(verts.size());
    for (int s = 0; s <= segments; ++s){
        float lon = static_cast<float>(s) * kPI2 / segments;
        float sinS = sinf(lon), cosS = cosf(lon);
        Vector2 uv(0.5f + 0.5f*sinS, 0.5f + 0.5f*cosS);
        verts.push_back({ {r*sinS, -halfH, r*cosS}, uv, {0,-1,0}, {1,0,0,1} });
    }
    for (int s = 0; s < segments; ++s){
        idx.push_back(botCentre);
        idx.push_back(botRingBase + s);
        idx.push_back(botRingBase + s + 1);
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, idx, 0);
    return mesh;
}
