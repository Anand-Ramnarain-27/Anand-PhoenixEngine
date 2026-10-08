#pragma once
// The scene host every loaded scene uses: an empty SceneGraph that the serializer or the editor fills.

#include "IScene.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include <memory>

class EmptyScene : public IScene {
public:
    EmptyScene() = default;
    ~EmptyScene() override = default;

    const char* getName() const override { return "Empty Scene"; }
    const char* getDescription() const override { return "A blank scene to start from scratch."; }

    bool initialize(ID3D12Device* device) override{
        m_scene = std::make_unique<SceneGraph>();
        return true;
    }

    void update(float deltaTime) override { m_scene->update(deltaTime); }

    void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera&, uint32_t, uint32_t) override{
        if (m_scene && m_scene->getRoot()) m_scene->getRoot()->render(cmd);
    }

    void shutdown() override { m_scene.reset(); }

    SceneGraph* getModuleScene() override { return m_scene.get(); }

private:
    std::unique_ptr<SceneGraph> m_scene;
};
