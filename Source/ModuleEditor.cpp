#include "Globals.h"
#include "ModuleEditor.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleResources.h"
#include "GraphicsSamplers.h"
#include "ModuleRingBuffer.h"

#include "ImGuiPass.h"
#include "DebugDrawPass.h"
#include "RenderTexture.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <chrono>

// Helper function to split string by delimiter
static std::vector<std::string> splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter))
    {
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

ModuleEditor::ModuleEditor()
{
    // Initialize settings
    m_settings = EditorSettings();
}

ModuleEditor::~ModuleEditor()
{
    // Clean up all exercises
    for (auto& pair : m_sceneStates)
    {
        if (pair.second.scene)
        {
            pair.second.scene->cleanUp();
        }
    }
    m_sceneStates.clear();
}

bool ModuleEditor::init()
{
    LOG("Initializing ModuleEditor...");

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
    {
        LOG("Failed to get D3D12 module");
        return false;
    }

    // Initialize ImGui pass for editor
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    if (!descriptors)
    {
        LOG("Failed to get shader descriptors");
        return false;
    }

    auto tableDesc = descriptors->allocTable();
    m_imguiPass = std::make_unique<ImGuiPass>(
        d3d12->getDevice(),
        d3d12->getHWnd(),
        tableDesc.getCPUHandle(),
        tableDesc.getGPUHandle()
    );

    // Initialize debug draw pass (optional)
    m_debugDrawPass = std::make_unique<DebugDrawPass>(
        d3d12->getDevice(),
        d3d12->getDrawCommandQueue(),
        false
    );

    // Set up ImGui style
    applyEditorStyle();

    // Create initial dock space
    createDockSpace();

    m_initialized = true;
    LOG("ModuleEditor initialized successfully");
    return true;
}

bool ModuleEditor::cleanUp()
{
    LOG("Cleaning up ModuleEditor...");

    // Clean up all loaded exercises
    for (auto& pair : m_sceneStates)
    {
        if (pair.second.scene)
        {
            pair.second.scene->cleanUp();
            pair.second.scene.reset();
        }
        pair.second.renderTexture.reset();
    }
    m_sceneStates.clear();

    // Clean up editor resources
    m_imguiPass.reset();
    m_debugDrawPass.reset();

    m_scene.clear();
    m_initialized = false;

    LOG("ModuleEditor cleaned up");
    return true;
}

void ModuleEditor::preRender()
{
    if (!m_initialized) return;

    // Start ImGui frame
    m_imguiPass->startFrame();

    // Update performance stats
    updatePerformanceStats();

    // Forward preRender to active exercise
    forwardPreRenderToScene();

    // Update viewer size for current exercise
    updateSceneRenderTexture();
}

void ModuleEditor::render()
{
    if (!m_initialized) return;

    // Apply editor style
    pushStyleColors();

    // Create dockspace
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("EditorDockspace", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoDocking);

    m_dockspaceID = ImGui::GetID("EditorDockspace");
    ImGui::DockSpace(m_dockspaceID, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::PopStyleVar();

    // Render menu bar (always on top)
    if (m_settings.showMenuBar && !m_settings.fullscreenViewer)
    {
        renderMenuBar();
    }

    // Render toolbar
    if (m_settings.showToolbar && !m_settings.fullscreenViewer)
    {
        renderToolbar();
    }

    // Render editor panels if not in fullscreen mode
    if (m_settings.showEditor && !m_settings.fullscreenViewer)
    {
        // Left panel: Exercise list and properties
        ImGui::Begin("Scene Panel", &m_settings.showEditor);
        renderScenePanel();
        ImGui::End();

        // Right panel: Various tools
        if (m_settings.showPropertyPanel)
        {
            ImGui::Begin("Properties", &m_settings.showPropertyPanel);
            renderPropertyPanel();
            ImGui::End();
        }

        if (m_settings.showSceneHierarchy)
        {
            ImGui::Begin("Scene Hierarchy", &m_settings.showSceneHierarchy);
            renderSceneHierarchy();
            ImGui::End();
        }

        if (m_settings.showStatsPanel)
        {
            ImGui::Begin("Statistics", &m_settings.showStatsPanel);
            renderStatsPanel();
            ImGui::End();
        }

        if (m_settings.showCameraControls)
        {
            ImGui::Begin("Camera", &m_settings.showCameraControls);
            renderCameraControls();
            ImGui::End();
        }

        if (m_settings.showLightingControls)
        {
            ImGui::Begin("Lighting", &m_settings.showLightingControls);
            renderLightingControls();
            ImGui::End();
        }

        if (m_settings.showTransformControls)
        {
            ImGui::Begin("Transform", &m_settings.showTransformControls);
            renderTransformControls();
            ImGui::End();
        }
    }

    // Viewer area (always visible)
    renderViewerArea();

    // Handle exercise input
    handleSceneInput();

    ImGui::End(); // End dockspace window

    popStyleColors();

    // Record ImGui commands
    ModuleD3D12* d3d12 = app->getD3D12();
    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();
    m_imguiPass->record(commandList);
}

void ModuleEditor::postRender()
{
    // Nothing to do here for now
}

void ModuleEditor::registerScene(const std::string& name,
    const std::string& description,
    std::function<std::unique_ptr<Module>()> factory,
    const std::string& category,
    bool supportsRenderTexture)
{
    SceneInfo info;
    info.name = name;
    info.description = description;
    info.factory = factory;
    info.category = category;
    info.supportsRenderTexture = supportsRenderTexture;
    info.loaded = false;

    m_scene.push_back(info);
    LOG("Registered Scene: %s (Category: %s)", name.c_str(), category.c_str());
}

bool ModuleEditor::loadScene(int index)
{
    if (index < 0 || index >= static_cast<int>(m_scene.size()))
    {
        LOG("Invalid scene index: %d", index);
        return false;
    }

    // Unload current active exercise if different
    if (m_activeScene != index && m_activeScene >= 0)
    {
        unloadScene(m_activeScene);
    }

    // Check if already loaded
    auto it = m_sceneStates.find(index);
    if (it != m_sceneStates.end() && it->second.isInitialized)
    {
        // Already loaded, just activate it
        m_activeScene = index;
        m_selectedScene = index;
        LOG("Activated already loaded scene: %s", m_scene[index].name.c_str());
        return true;
    }

    // Create new exercise
    SceneState newState;
    newState.scene = m_scene[index].factory();

    if (!newState.scene)
    {
        LOG("Failed to create scene: %s", m_scene[index].name.c_str());
        return false;
    }

    // Initialize the exercise
    if (!newState.scene->init())
    {
        LOG("Failed to initialize scene: %s", m_scene[index].name.c_str());
        newState.scene.reset();
        return false;
    }

    // Create render texture for the exercise
    if (m_scene[index].supportsRenderTexture)
    {
        newState.renderTexture = std::make_unique<RenderTexture>(
            m_scene[index].name,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vector4(0.188f, 0.208f, 0.259f, 1.0f),
            DXGI_FORMAT_D32_FLOAT,
            1.0f
        );
    }

    newState.isInitialized = true;
    newState.lastViewerSize = ImVec2(0, 0);

    // Store the state
    m_sceneStates[index] = std::move(newState);
    m_activeScene = index;
    m_selectedScene = index;
    m_scene[index].loaded = true;

    LOG("Successfully loaded scene: %s", m_scene[index].name.c_str());
    return true;
}

void ModuleEditor::unloadScene(int index)
{
    auto it = m_sceneStates.find(index);
    if (it != m_sceneStates.end())
    {
        if (it->second.scene)
        {
            it->second.scene->cleanUp();
            it->second.scene.reset();
        }
        it->second.renderTexture.reset();
        m_sceneStates.erase(it);
        m_scene[index].loaded = false;

        if (m_activeScene == index)
        {
            m_activeScene = -1;
        }

        LOG("Unloaded scene: %s", m_scene[index].name.c_str());
    }
}

bool ModuleEditor::isSceneLoaded(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_scene.size()))
        return false;

    return m_scene[index].loaded;
}

const ModuleEditor::SceneInfo* ModuleEditor::getSceneInfo(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_scene.size()))
        return nullptr;

    return &m_scene[index];
}

int ModuleEditor::getSceneCount() const
{
    return static_cast<int>(m_scene.size());
}

Module* ModuleEditor::getActiveScene() const
{
    auto it = m_sceneStates.find(m_activeScene);
    if (it != m_sceneStates.end() && it->second.scene)
    {
        return it->second.scene.get();
    }
    return nullptr;
}

bool ModuleEditor::createDockSpace()
{
    // This will be called from render() to create/update the dockspace
    return true;
}

void ModuleEditor::renderMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) { /* TODO */ }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Layout", nullptr, false)) { saveLayout(); }
            if (ImGui::MenuItem("Load Layout", nullptr, false)) { loadLayout(); }
            if (ImGui::MenuItem("Reset Layout", nullptr, false)) { resetLayout(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) { /* TODO: Request application exit */ }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) { /* TODO */ }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, false)) { /* TODO */ }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, false)) { /* TODO */ }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) { /* TODO */ }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Editor", nullptr, &m_settings.showEditor);
            ImGui::MenuItem("Fullscreen Viewer", "F11", &m_settings.fullscreenViewer);
            ImGui::Separator();
            ImGui::MenuItem("Menu Bar", nullptr, &m_settings.showMenuBar, true);
            ImGui::MenuItem("Toolbar", nullptr, &m_settings.showToolbar, true);
            ImGui::Separator();
            ImGui::MenuItem("Properties Panel", nullptr, &m_settings.showPropertyPanel, true);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_settings.showSceneHierarchy, true);
            ImGui::MenuItem("Statistics Panel", nullptr, &m_settings.showStatsPanel, true);
            ImGui::MenuItem("Camera Controls", nullptr, &m_settings.showCameraControls, true);
            ImGui::MenuItem("Lighting Controls", nullptr, &m_settings.showLightingControls, true);
            ImGui::MenuItem("Transform Controls", nullptr, &m_settings.showTransformControls, true);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scenes"))
        {
            std::string currentCategory;
            for (size_t i = 0; i < m_scene.size(); ++i)
            {
                if (hasCategoryChanged(i, currentCategory))
                {
                    if (!currentCategory.empty())
                    {
                        ImGui::EndMenu();
                    }
                    currentCategory = m_scene[i].category;
                    if (ImGui::BeginMenu(currentCategory.c_str()))
                    {
                        // Menu items will be added below
                    }
                    else
                    {
                        // Skip this category if menu not opened
                        currentCategory.clear();
                        continue;
                    }
                }

                bool isLoaded = m_scene[i].loaded;
                bool isActive = (m_activeScene == static_cast<int>(i));

                std::string menuText = m_scene[i].name;
                if (isActive) menuText += " *";

                if (ImGui::MenuItem(menuText.c_str(), nullptr, isActive))
                {
                    loadScene(static_cast<int>(i));
                }

                if (ImGui::IsItemHovered() && !m_scene[i].description.empty())
                {
                    ImGui::SetTooltip("%s", m_scene[i].description.c_str());
                }
            }

            if (!currentCategory.empty())
            {
                ImGui::EndMenu(); // Close last category
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About", nullptr, false)) { /* TODO */ }
            if (ImGui::MenuItem("Documentation", nullptr, false)) { /* TODO */ }
            if (ImGui::MenuItem("Keyboard Shortcuts", nullptr, false)) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Report Issue", nullptr, false)) { /* TODO */ }
            ImGui::EndMenu();
        }

        // Right-aligned status info
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::Text("FPS: %d | Frame: %.2f ms", m_fps, m_frameTime);

        ImGui::EndMainMenuBar();
    }
}

void ModuleEditor::renderToolbar()
{
    ImGui::Begin("Toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    // Toolbar size and position
    ImVec2 toolbarSize = ImVec2(ImGui::GetWindowWidth(), 40);
    ImGui::SetWindowSize(toolbarSize);

    // Center tools horizontally
    float totalWidth = 0;
    int buttonCount = 6; // Adjust based on actual buttons

    // Calculate total width and center
    ImGuiStyle& style = ImGui::GetStyle();
    totalWidth = (buttonCount * 40) + ((buttonCount - 1) * style.ItemSpacing.x);
    float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
    ImGui::SetCursorPosX(startX);

    // Tool buttons
    if (ImGui::Button("Select", ImVec2(40, 30))) { /* TODO */ }
    ImGui::SameLine();
    if (ImGui::Button("Move", ImVec2(40, 30))) { /* TODO */ }
    ImGui::SameLine();
    if (ImGui::Button("Rotate", ImVec2(40, 30))) { /* TODO */ }
    ImGui::SameLine();
    if (ImGui::Button("Scale", ImVec2(40, 30))) { /* TODO */ }
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    if (ImGui::Button("Play", ImVec2(40, 30))) { /* TODO */ }
    ImGui::SameLine();
    if (ImGui::Button("Pause", ImVec2(40, 30))) { /* TODO */ }

    ImGui::End();
}

void ModuleEditor::renderScenePanel()
{
    ImGui::Text("Scenes");
    ImGui::Separator();

    // Filter input
    static char filter[256] = "";
    ImGui::InputTextWithHint("##Filter", "Filter exercises...", filter, sizeof(filter));

    // Exercise list
    ImGui::BeginChild("SceneList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);

    std::string currentCategory;
    bool categoryOpen = false;

    for (size_t i = 0; i < m_scene.size(); ++i)
    {
        // Filter by name
        if (filter[0] != '\0' &&
            m_scene[i].name.find(filter) == std::string::npos &&
            m_scene[i].description.find(filter) == std::string::npos)
        {
            continue;
        }

        // Handle category separators
        if (hasCategoryChanged(i, currentCategory))
        {
            if (categoryOpen)
            {
                ImGui::TreePop();
            }

            currentCategory = m_scene[i].category;
            categoryOpen = ImGui::TreeNodeEx(currentCategory.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen);

            if (!categoryOpen)
            {
                // Skip entire category
                while (i + 1 < m_scene.size() &&
                    m_scene[i + 1].category == currentCategory)
                {
                    i++;
                }
                currentCategory.clear();
                continue;
            }
        }

        if (!categoryOpen && !currentCategory.empty())
        {
            continue;
        }

        // Exercise item
        bool isSelected = (m_selectedScene == static_cast<int>(i));
        bool isActive = (m_activeScene == static_cast<int>(i));
        bool isLoaded = m_scene[i].loaded;

        // Create a selectable item with status indicator
        ImGui::PushID(static_cast<int>(i));

        ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (isActive) textColor = m_settings.highlightColor;
        else if (isLoaded) textColor = ImVec4(0.8f, 0.8f, 0.4f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        std::string displayName = m_scene[i].name;
        if (isActive) displayName += " [Active]";
        else if (isLoaded) displayName += " [Loaded]";

        if (ImGui::Selectable(displayName.c_str(), isSelected))
        {
            m_selectedScene = static_cast<int>(i);
        }

        ImGui::PopStyleColor();

        // Tooltip with description
        if (ImGui::IsItemHovered() && !m_scene[i].description.empty())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", m_scene[i].description.c_str());
            ImGui::EndTooltip();
        }

        // Double-click to load
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            loadScene(static_cast<int>(i));
        }

        // Context menu
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Load"))
            {
                loadScene(static_cast<int>(i));
            }

            if (isLoaded)
            {
                if (ImGui::MenuItem("Unload"))
                {
                    unloadScene(static_cast<int>(i));
                }
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (categoryOpen)
    {
        ImGui::TreePop();
    }

    ImGui::EndChild();

    // Load/Unload buttons
    ImGui::Separator();

    if (m_selectedScene >= 0 && m_selectedScene < static_cast<int>(m_scene.size()))
    {
        bool isLoaded = m_scene[m_selectedScene].loaded;

        if (isLoaded)
        {
            if (ImGui::Button("Unload Selected", ImVec2(-1, 0)))
            {
                unloadScene(m_selectedScene);
            }

            if (m_activeScene != m_selectedScene)
            {
                ImGui::SameLine();
                if (ImGui::Button("Activate", ImVec2(-1, 0)))
                {
                    loadScene(m_selectedScene);
                }
            }
        }
        else
        {
            if (ImGui::Button("Load Selected", ImVec2(-1, 0)))
            {
                loadScene(m_selectedScene);
            }
        }
    }
    else
    {
        ImGui::Text("No scene selected");
    }
}

void ModuleEditor::renderPropertyPanel()
{
    ImGui::Text("Properties");
    ImGui::Separator();

    if (m_activeScene >= 0)
    {
        ImGui::Text("Active Scene: %s", m_scene[m_activeScene].name.c_str());
        ImGui::Separator();

        // Here you would render exercise-specific properties
        // This would require exercise to implement a property interface
        ImGui::Text("Scene-specific properties");
        ImGui::Text("would appear here.");
    }
    else
    {
        ImGui::Text("No active Scene");
        ImGui::Text("Select and load an Scene");
        ImGui::Text("from the Scene Panel.");
    }
}

void ModuleEditor::renderSceneHierarchy()
{
    ImGui::Text("Scene Hierarchy");
    ImGui::Separator();

    // Placeholder for scene hierarchy
    // This would be populated by the active exercise
    ImGui::Text("Scene hierarchy would");
    ImGui::Text("appear here when an");
    ImGui::Text("exercise is loaded.");
}

void ModuleEditor::renderStatsPanel()
{
    ImGui::Text("Statistics");
    ImGui::Separator();

    // Performance stats
    ImGui::Text("Performance:");
    ImGui::Text("  FPS: %d", m_fps);
    ImGui::Text("  Frame Time: %.2f ms", m_frameTime);
    ImGui::Text("  Frame Count: %zu", m_frameCount);

    ImGui::Separator();

    // Memory stats (placeholder)
    ImGui::Text("Memory:");
    ImGui::Text("  Scene Loaded: %zu", m_sceneStates.size());
    ImGui::Text("  Total Scene: %zu", m_scene.size());

    if (m_activeScene >= 0)
    {
        ImGui::Separator();
        ImGui::Text("Active Scene:");
        ImGui::Text("  %s", m_scene[m_activeScene].name.c_str());
        ImGui::Text("  Viewer Size: %.0f x %.0f",
            m_viewerSize.x, m_viewerSize.y);
    }
}

void ModuleEditor::renderViewerArea()
{
    // Viewer window flags
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (m_settings.fullscreenViewer)
    {
        flags |= ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        // Fullscreen viewer takes the entire window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    }

    ImGui::Begin("Viewer", nullptr, flags);

    // Store viewer state
    m_viewerFocused = ImGui::IsWindowFocused();
    m_viewerHovered = ImGui::IsWindowHovered();
    m_viewerPos = ImGui::GetWindowPos();
    m_viewerSize = ImGui::GetContentRegionAvail();

    // Toggle fullscreen button
    if (!m_settings.fullscreenViewer)
    {
        if (ImGui::Button(m_settings.showEditor ? "Hide Editor" : "Show Editor"))
        {
            m_settings.showEditor = !m_settings.showEditor;
        }

        ImGui::SameLine();

        if (ImGui::Button("Fullscreen (F11)"))
        {
            m_settings.fullscreenViewer = true;
            m_settings.showEditor = false;
        }
    }
    else
    {
        // Exit fullscreen button (centered)
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 buttonSize = ImVec2(200, 40);
        ImVec2 buttonPos = ImVec2((windowSize.x - buttonSize.x) * 0.5f,
            (windowSize.y - buttonSize.y) * 0.5f);

        ImGui::SetCursorPos(buttonPos);

        if (ImGui::Button("Exit Fullscreen (F11)", buttonSize))
        {
            m_settings.fullscreenViewer = false;
            m_settings.showEditor = true;
        }
    }

    // Render the active exercise
    if (m_activeScene >= 0)
    {
        renderSceneToViewer();
    }
    else
    {
        // No exercise loaded - show placeholder
        ImVec2 textSize = ImGui::CalcTextSize("No Scene loaded");
        ImVec2 centerPos = ImVec2(
            (m_viewerSize.x - textSize.x) * 0.5f,
            (m_viewerSize.y - textSize.y) * 0.5f
        );

        ImGui::SetCursorPos(centerPos);
        ImGui::Text("No Scene loaded");

        ImGui::SetCursorPos(ImVec2(centerPos.x, centerPos.y + textSize.y + 10));
        ImGui::Text("Select an Scene from the Scene Panel");
    }

    ImGui::End();
}

void ModuleEditor::renderCameraControls()
{
    ImGui::Text("Camera Controls");
    ImGui::Separator();

    ModuleCamera* camera = app->getCamera();
    if (!camera)
    {
        ImGui::Text("Camera not available");
        return;
    }

    Vector3 camPos = camera->getPos();
    float polar = camera->getPolar();
    float azimuthal = camera->getAzimuthal();

    // Position controls
    bool cameraChanged = false;
    cameraChanged |= ImGui::DragFloat3("Position", &camPos.x, 0.1f);

    // Rotation controls (in degrees)
    float polarDeg = XMConvertToDegrees(polar);
    float azimuthalDeg = XMConvertToDegrees(azimuthal);

    cameraChanged |= ImGui::DragFloat("Polar", &polarDeg, 1.0f, -180.0f, 180.0f);
    cameraChanged |= ImGui::DragFloat("Azimuthal", &azimuthalDeg, 1.0f, -180.0f, 180.0f);

    if (cameraChanged)
    {
        camera->setTranslation(camPos);
        camera->setPolar(XMConvertToRadians(polarDeg));
        camera->setAzimuthal(XMConvertToRadians(azimuthalDeg));
    }

    // Reset button
    if (ImGui::Button("Reset Camera"))
    {
        camera->setTranslation(Vector3(0, 0, 5));
        camera->setPolar(0);
        camera->setAzimuthal(0);
    }

    // Camera info
    ImGui::Separator();
    ImGui::Text("Camera Info:");
    ImGui::Text("  Forward: %.2f, %.2f, %.2f",
        camera->getForward().x,
        camera->getForward().y,
        camera->getForward().z);
    ImGui::Text("  Right: %.2f, %.2f, %.2f",
        camera->getRight().x,
        camera->getRight().y,
        camera->getRight().z);
    ImGui::Text("  Up: %.2f, %.2f, %.2f",
        camera->getUp().x,
        camera->getUp().y,
        camera->getUp().z);
}

void ModuleEditor::renderLightingControls()
{
    ImGui::Text("Lighting Controls");
    ImGui::Separator();

    // Placeholder lighting controls
    // These would need to be connected to the active exercise
    static Vector3 lightDirection = Vector3(-0.5f, -1.0f, -0.5f);
    static Vector3 lightColor = Vector3(1.0f, 1.0f, 1.0f);
    static float lightIntensity = 1.0f;
    static Vector3 ambientColor = Vector3(0.1f, 0.1f, 0.1f);

    ImGui::DragFloat3("Light Direction", &lightDirection.x, 0.01f, -1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", &lightColor.x);
    ImGui::DragFloat("Light Intensity", &lightIntensity, 0.1f, 0.0f, 10.0f);
    ImGui::ColorEdit3("Ambient Color", &ambientColor.x);

    if (ImGui::Button("Normalize Direction"))
    {
        lightDirection.Normalize();
    }

    // These values would be sent to the active exercise
    // For now, they're just stored locally
}

void ModuleEditor::renderTransformControls()
{
    ImGui::Text("Transform Controls");
    ImGui::Separator();

    // Placeholder transform controls
    static Vector3 translation = Vector3(0, 0, 0);
    static Vector3 rotation = Vector3(0, 0, 0);
    static Vector3 scale = Vector3(1, 1, 1);

    ImGui::DragFloat3("Translation", &translation.x, 0.1f);
    ImGui::DragFloat3("Rotation", &rotation.x, 1.0f, -180.0f, 180.0f);
    ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.01f, 10.0f);

    // These transforms would be applied to selected objects
    // in the active exercise
}

void ModuleEditor::renderSceneToViewer()
{
    auto it = m_sceneStates.find(m_activeScene);
    if (it == m_sceneStates.end() || !it->second.scene)
    {
        ImGui::Text("Error: Scene not properly loaded");
        return;
    }

    SceneState& state = it->second;

    // Check if we need to resize the render texture
    if (state.renderTexture &&
        (state.lastViewerSize.x != m_viewerSize.x ||
            state.lastViewerSize.y != m_viewerSize.y))
    {
        if (m_viewerSize.x > 0 && m_viewerSize.y > 0)
        {
            state.renderTexture->resize(
                static_cast<int>(m_viewerSize.x),
                static_cast<int>(m_viewerSize.y)
            );
            state.lastViewerSize = m_viewerSize;
        }
    }

    // Display the render texture
    if (state.renderTexture && state.renderTexture->isValid())
    {
        // Get the SRV handle for the render texture
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = state.renderTexture->getSrvHandle();

        // Display as an ImGui image
        ImGui::Image(
            reinterpret_cast<ImTextureID>(srvHandle.ptr),
            m_viewerSize,
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
    }
    else
    {
        // Fallback: Just draw a colored rectangle
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max = ImVec2(min.x + m_viewerSize.x, min.y + m_viewerSize.y);
        drawList->AddRectFilled(min, max, IM_COL32(40, 40, 40, 255));

        // Show message
        ImVec2 textSize = ImGui::CalcTextSize("Scene rendering...");
        ImVec2 textPos = ImVec2(
            min.x + (m_viewerSize.x - textSize.x) * 0.5f,
            min.y + (m_viewerSize.y - textSize.y) * 0.5f
        );
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "Scene rendering...");
    }
}

void ModuleEditor::setupViewerRendering()
{
    // This would set up the command list for rendering to texture
    // Actual rendering is done by the exercise's render() method
}

void ModuleEditor::cleanupViewerRendering()
{
    // Clean up any temporary resources
}

void ModuleEditor::forwardPreRenderToScene()
{
    if (m_activeScene >= 0)
    {
        auto it = m_sceneStates.find(m_activeScene);
        if (it != m_sceneStates.end() && it->second.scene)
        {
            // Call the exercise's preRender method
            it->second.scene->preRender();
        }
    }
}

void ModuleEditor::handleSceneInput()
{
    // Forward input to active exercise if viewer is focused
    if (m_viewerFocused && m_activeScene >= 0)
    {
        // Enable camera control
        ModuleCamera* camera = app->getCamera();
        if (camera)
        {
            camera->setEnable(true);
        }

        // TODO: Forward other input events to exercise
    }
    else
    {
        // Disable camera control when not focused
        ModuleCamera* camera = app->getCamera();
        if (camera)
        {
            camera->setEnable(false);
        }
    }
}

void ModuleEditor::updateSceneRenderTexture()
{
    if (m_activeScene >= 0)
    {
        auto it = m_sceneStates.find(m_activeScene);
        if (it != m_sceneStates.end() && it->second.renderTexture)
        {
            // Update render texture size if viewer size changed
            if (it->second.lastViewerSize.x != m_viewerSize.x ||
                it->second.lastViewerSize.y != m_viewerSize.y)
            {
                if (m_viewerSize.x > 0 && m_viewerSize.y > 0)
                {
                    it->second.renderTexture->resize(
                        static_cast<int>(m_viewerSize.x),
                        static_cast<int>(m_viewerSize.y)
                    );
                    it->second.lastViewerSize = m_viewerSize;
                }
            }
        }
    }
}

void ModuleEditor::updatePerformanceStats()
{
    // Update frame time and FPS
    double currentTime = ImGui::GetTime();
    double deltaTime = currentTime - m_lastTime;

    if (deltaTime > 0)
    {
        m_frameTime = static_cast<float>(deltaTime * 1000.0); // Convert to ms
        m_fps = static_cast<int>(1.0 / deltaTime);
    }

    m_lastTime = currentTime;
    m_frameCount++;
}

void ModuleEditor::renderHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ModuleEditor::renderCategorySeparator(const std::string& category)
{
    ImGui::Separator();
    ImGui::Text("%s", category.c_str());
    ImGui::Separator();
}

bool ModuleEditor::hasCategoryChanged(int index, const std::string& currentCategory)
{
    if (index < 0 || index >= static_cast<int>(m_scene.size()))
        return false;

    return m_scene[index].category != currentCategory;
}

void ModuleEditor::applyEditorStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.11f, 0.11f, 0.94f);

    // Borders
    style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frame
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

    // Title
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

    // Scrollbar
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Sliders
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    // Buttons
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

    // Headers
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

    // Separator
    style.Colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    // Resize grip
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.30f, 0.29f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.50f, 0.50f, 0.95f);

    // Docking
    style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // Tab
    style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

    // Plot lines
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);

    // Text
    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Checkbox
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    // Sizing
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(4, 3);
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 21;
    style.ScrollbarSize = 14;
    style.GrabMinSize = 12;

    // Borders
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.TabBorderSize = 0;

    // Rounding
    style.WindowRounding = 4;
    style.ChildRounding = 4;
    style.FrameRounding = 2;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 2;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;

    // Alignment
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ColorButtonPosition = ImGuiDir_Left;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    // Safe area padding
    style.DisplaySafeAreaPadding = ImVec2(3, 3);
}

void ModuleEditor::pushStyleColors()
{
    // Save current style colors and push editor colors
    // This is done at the beginning of render()
}

void ModuleEditor::popStyleColors()
{
    // Restore original style colors
    // This is done at the end of render()
}

void ModuleEditor::setupDefaultLayout()
{
    // Set up default docking layout
    // This would be called when no saved layout exists
}

void ModuleEditor::saveLayout()
{
    // Save current layout to disk
    LOG("Saving editor layout...");
    // TODO: Implement layout saving
}

void ModuleEditor::loadLayout()
{
    // Load layout from disk
    LOG("Loading editor layout...");
    // TODO: Implement layout loading
}

void ModuleEditor::resetLayout()
{
    // Reset to default layout
    LOG("Resetting editor layout to default...");
    setupDefaultLayout();
}