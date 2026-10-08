#pragma once
// Interface of a scene the SceneManager can host.

#include <cstdint>
#include <d3d12.h>

class ModuleCamera;
class SceneGraph;

/// A hosted scene. Every scene the engine loads is an EmptyScene filled from a scene file; getModuleScene()
/// exposes its SceneGraph to the rest of the engine.
class IScene {
public:
    virtual ~IScene() = default;

    virtual const char* getName() const = 0;
    virtual const char* getDescription() const { return nullptr; }

    virtual bool initialize(ID3D12Device* device) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t width, uint32_t height) = 0;
    virtual void shutdown() = 0;

    virtual void reset(){}
    virtual void onEnter(){}
    virtual void onExit(){}
    virtual void onViewportResized(uint32_t, uint32_t){}
    virtual bool wantsDebugDraw() const { return true; }
    virtual SceneGraph* getModuleScene(){ return nullptr; }
};
