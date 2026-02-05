#pragma once

#include "Module.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

// Forward declarations
class RenderTexture;
class DebugDrawPass;
class ImGuiPass;
class BasicModel;
class ModuleCamera;

class ModuleEditor : public Module
{
public:
    // Structure to hold exercise information
    struct SceneInfo
    {
        std::string name;
        std::string description;
        std::function<std::unique_ptr<Module>()> factory;
        std::string category;
        bool loaded = false;
        bool supportsRenderTexture = true;
    };

    // Editor settings
    struct EditorSettings
    {
        bool showEditor = true;
        bool fullscreenViewer = false;
        bool showMenuBar = true;
        bool showToolbar = true;
        bool showScenePanel = true;
        bool showPropertyPanel = true;
        bool showSceneHierarchy = false;
        bool showStatsPanel = true;
        bool showCameraControls = true;
        bool showLightingControls = true;
        bool showTransformControls = true;

        // Layout
        float editorPanelWidth = 300.0f;
        ImVec2 lastWindowSize;

        // Colors
        ImVec4 backgroundColor = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        ImVec4 highlightColor = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
    };

    // Exercise state
    struct SceneState
    {
        std::unique_ptr<Module> scene;
        std::unique_ptr<RenderTexture> renderTexture;
        ImVec2 lastViewerSize;
        bool isInitialized = false;
    };

private:
    // Exercise management
    std::vector<SceneInfo> m_scene;
    std::unordered_map<int, SceneState> m_sceneStates; // Keyed by exercise index
    int m_selectedScene = -1;
    int m_activeScene = -1;

    // Editor UI components
    std::unique_ptr<ImGuiPass> m_imguiPass;
    std::unique_ptr<DebugDrawPass> m_debugDrawPass;

    // Editor state
    EditorSettings m_settings;
    bool m_initialized = false;

    // UI state
    ImVec2 m_viewerSize;
    ImVec2 m_viewerPos;
    bool m_viewerFocused = false;
    bool m_viewerHovered = false;

    // Performance tracking
    float m_frameTime = 0.0f;
    int m_fps = 0;
    size_t m_frameCount = 0;
    double m_lastTime = 0.0;

    // Layout IDs
    ImGuiID m_dockspaceID = 0;
    ImGuiID m_dockLeft = 0;
    ImGuiID m_dockRight = 0;
    ImGuiID m_dockBottom = 0;

public:
    ModuleEditor();
    ~ModuleEditor();

    virtual bool init() override;
    virtual bool cleanUp() override;
    virtual void preRender() override;
    virtual void render() override;
    virtual void postRender() override;

    // Exercise management
    void registerScene(const std::string& name,
        const std::string& description,
        std::function<std::unique_ptr<Module>()> factory,
        const std::string& category = "General",
        bool supportsRenderTexture = true);

    bool loadScene(int index);
    void unloadScene(int index);
    bool isSceneLoaded(int index) const;
    const SceneInfo* getSceneInfo(int index) const;
    int getSceneCount() const;

    // Editor control
    void toggleEditorVisibility() { m_settings.showEditor = !m_settings.showEditor; }
    void toggleFullscreenViewer() { m_settings.fullscreenViewer = !m_settings.fullscreenViewer; }
    void setEditorVisibility(bool visible) { m_settings.showEditor = visible; }
    void setFullscreenViewer(bool fullscreen) { m_settings.fullscreenViewer = fullscreen; }

    // Accessors
    bool isViewerFocused() const { return m_viewerFocused; }
    bool isViewerHovered() const { return m_viewerHovered; }
    ImVec2 getViewerSize() const { return m_viewerSize; }
    ImVec2 getViewerPos() const { return m_viewerPos; }
    Module* getActiveScene() const;

private:
    // Initialization
    bool initializeEditor();
    bool createDockSpace();

    // UI Rendering
    void renderMenuBar();
    void renderToolbar();
    void renderScenePanel();
    void renderPropertyPanel();
    void renderSceneHierarchy();
    void renderStatsPanel();
    void renderViewerArea();
    void renderCameraControls();
    void renderLightingControls();
    void renderTransformControls();

    // Viewer rendering
    void renderSceneToViewer();
    void setupViewerRendering();
    void cleanupViewerRendering();

    // Layout management
    void setupDefaultLayout();
    void saveLayout();
    void loadLayout();
    void resetLayout();

    // Exercise communication
    void forwardPreRenderToScene();
    void handleSceneInput();
    void updateSceneRenderTexture();

    // Helper functions
    void updatePerformanceStats();
    void renderHelpMarker(const char* desc);
    void renderCategorySeparator(const std::string& category);
    bool hasCategoryChanged(int index, const std::string& currentCategory);

    // Style
    void applyEditorStyle();
    void pushStyleColors();
    void popStyleColors();
};