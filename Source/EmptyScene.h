#pragma once

#include "IScene.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include <memory>

class ModuleScene;

class EmptyScene : public IScene
{
public:
    EmptyScene() = default;
    ~EmptyScene() override = default;

    const char* getName() const override { return "Empty Scene"; }
    const char* getDescription() const override { return "A blank scene to start from scratch."; }

    bool initialize(ID3D12Device* device) override
    {
        scene = std::make_unique<ModuleScene>();
        return true;
    }

    void update(float deltaTime) override
    {
        scene->update(deltaTime);
    }

    void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera&, uint32_t, uint32_t) override
    {
        if (scene && scene->getRoot())
        {
            scene->getRoot()->render(cmd);
        }
    }

    void shutdown() override
    {
        scene.reset();
    }

    ModuleScene* getModuleScene() override { return scene.get(); }

private:
    std::unique_ptr<ModuleScene> scene;
};