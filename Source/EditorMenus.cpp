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

static constexpr float kStatusH = 22.f;

void ModuleEditor::drawDockspace(){
    constexpr ImGuiWindowFlags kF =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - kStatusH));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Dock", nullptr, kF);
    ImGui::PopStyleVar(3);

    ImGuiID dock = ImGui::GetID("DS");
    ImGui::DockSpace(dock, ImVec2(0, 0), ImGuiDockNodeFlags_None);

    if (m_firstFrame){
        m_firstFrame = false;

        const float vpW = vp->WorkSize.x;
        const float vpH = vp->WorkSize.y - kStatusH;
        const float leftRatio = 220.f / vpW;
        const float rightRatio = 280.f / (vpW - 220.f);
        const float bottomRatio = 200.f / vpH;

        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImVec2(vpW, vpH));

        ImGuiID left, mid, right, bottom;
        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, leftRatio, &left, &mid);
        ImGui::DockBuilderSplitNode(mid, ImGuiDir_Right, rightRatio, &right, &mid);
        ImGui::DockBuilderSplitNode(mid, ImGuiDir_Down, bottomRatio, &bottom, &mid);

        ImGuiID leftTop, leftBot;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.22f, &leftBot, &leftTop);
        ImGui::DockBuilderDockWindow("Hierarchy", leftTop);
        ImGui::DockBuilderDockWindow("Scene Settings", leftBot);

        ImGui::DockBuilderDockWindow("Viewport", mid);
        ImGui::DockBuilderDockWindow("Game", mid);

        ImGuiID rightTop, rightBot;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.42f, &rightBot, &rightTop);
        ImGui::DockBuilderDockWindow("Inspector", rightTop);
        ImGui::DockBuilderDockWindow("GPU Memory", rightBot);

        ImGui::DockBuilderDockWindow("Render Graph", bottom);
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);
        ImGui::DockBuilderDockWindow("Collision Debug", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Performance", bottom);
        ImGui::DockBuilderDockWindow("Resources", bottom);

        ImGui::DockBuilderFinish(dock);
    }
    ImGui::End();
}

void ModuleEditor::drawMenuBar(){
    if (!ImGui::BeginMainMenuBar()) return;

    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::Text("Phoenix");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::Text("v0.4");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8);
    }

    auto saveScene = [&](){
        if (!m_currentScenePath.empty() && m_sceneManager->getActiveScene()){
            bool ok = m_sceneManager->saveCurrentScene(m_currentScenePath);
            log(ok ? "Scene saved!" : "Failed to save.", ok ? EditorColors::Success : EditorColors::Danger);
        }
        else m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
    };

    if (ImGui::BeginMenu("File")){
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) m_showNewSceneConfirm = true;
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")){}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")){
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo())) undoToSavePoint();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selection.has())) copySelected();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.serialized.empty())) pasteClipboard();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selection.has())) duplicateSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N")) createEmptyGameObject();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")){
        if (ImGui::MenuItem("Create Empty")) createEmptyGameObject();
        if (ImGui::MenuItem("Create Empty Child") && m_selection.has()) createEmptyGameObject("Empty", m_selection.object);
        ImGui::Separator();
        if (ImGui::BeginMenu("Primitives")){
            if (ImGui::MenuItem("Cube")) spawnPrimitive(PrimitiveType::Cube);
            if (ImGui::MenuItem("Sphere")) spawnPrimitive(PrimitiveType::Sphere);
            if (ImGui::MenuItem("Capsule")) spawnPrimitive(PrimitiveType::Capsule);
            if (ImGui::MenuItem("Plane")) spawnPrimitive(PrimitiveType::Plane);
            if (ImGui::MenuItem("Cylinder")) spawnPrimitive(PrimitiveType::Cylinder);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Lights")){
            auto spawnLight = [&](const char* name, Component::Type type){
                SceneGraph* sc = getActiveModuleScene();
                if (!sc) return;
                GameObject* go = sc->createGameObject(name);
                go->addComponent(ComponentFactory::CreateComponent(type, go));
                m_selection.object = go;
                log((std::string("Created ") + name).c_str(), EditorColors::Success);
            };
            if (ImGui::MenuItem("Directional Light")) spawnLight("Directional Light", Component::Type::DirectionalLight);
            if (ImGui::MenuItem("Point Light")) spawnLight("Point Light", Component::Type::PointLight);
            if (ImGui::MenuItem("Spot Light")) spawnLight("Spot Light", Component::Type::SpotLight);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Particle Effects")){
            if (ImGui::MenuItem("Fire (Exercise 1)"))
                spawnFireParticleSystem(Vector3(0.f, 0.f, 0.f));
            if (ImGui::MenuItem("Sword Trail"))
                spawnSwordTrail(Vector3(0.f, 0.f, 0.f));
            ImGui::Separator();
            if (ImGui::MenuItem("Fire Comet (Trail + Particles Prefab)"))
                spawnFireComet(Vector3(0.f, 1.5f, 0.f));
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Random Primitive + Physics", "Shift+P")){
            static int menuSpawnIdx = 0; ++menuSpawnIdx;
            static const PrimitiveType kT[] = { PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Capsule, PrimitiveType::Cylinder };
            spawnPrimitive(kT[menuSpawnIdx % 4],
                Vector3((float)((menuSpawnIdx*3)%11-5), 5.f, (float)((menuSpawnIdx*7)%11-5)),
                Vector3::One, true);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Component")){
        auto addToSel = [&](const char* label, Component::Type type){
            if (!m_selection.has()) return;
            if (ImGui::MenuItem(label)){
                m_selection.object->addComponent(ComponentFactory::CreateComponent(type, m_selection.object));
                log((std::string("Added ") + label).c_str(), EditorColors::Success);
            }
        };
        addToSel("Mesh", Component::Type::Mesh);
        addToSel("Rigidbody", Component::Type::Rigidbody);
        addToSel("Camera", Component::Type::Camera);
        addToSel("Directional Light", Component::Type::DirectionalLight);
        addToSel("Point Light", Component::Type::PointLight);
        addToSel("Spot Light", Component::Type::SpotLight);
        addToSel("Animation", Component::Type::Animation);
        addToSel("Bounds", Component::Type::Bounds);
        addToSel("Decal", Component::Type::Decal);
        addToSel("Billboard", Component::Type::Billboard);
        addToSel("Particle System", Component::Type::ParticleSystem);
        addToSel("Trail", Component::Type::Trail);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Debug")){
        if (m_sceneManager){
            EditorSceneSettings& s = m_sceneManager->getSettings();
            ImGui::MenuItem("AABB Bounding Volumes", nullptr, &s.debugDrawBounds);
            ImGui::MenuItem("Broadphase Grid", nullptr, &s.debugDrawGrid);
            ImGui::MenuItem("Show Light Proxies", nullptr, &s.debugDrawLights);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Camera & Culling")){
            ModuleCamera* cam = app->getCamera();
            if (cam){
                ImGui::Text("Active Game Camera");
                GameObject* activeCamGO = cam->getActiveCamera();
                const char* preview = activeCamGO ? activeCamGO->getName().c_str() : "(none)";
                if (ImGui::BeginCombo("##ActiveGameCamera", preview)){
                    if (ImGui::Selectable("(none)", activeCamGO == nullptr)){
                        cam->setActiveCamera(nullptr);
                        cam->clearGameCameraFrustum();
                    }
                    if (SceneGraph* scene = getActiveModuleScene()){
                        std::function<void(GameObject*)> listCams = [&](GameObject* node){
                            if (!node) return;
                            if (node->getComponent<ComponentCamera>()){
                                bool selected = (node == activeCamGO);
                                if (ImGui::Selectable(node->getName().c_str(), selected))
                                    cam->setActiveCamera(node);
                            }
                            for (auto* child : node->getChildren()) listCams(child);
                        };
                        listCams(scene->getRoot());
                    }
                    ImGui::EndCombo();
                }
                ImGui::Separator();

                int cm = (int)cam->cullMode;
                ImGui::Text("Cull Mode"); ImGui::SameLine();
                if (ImGui::RadioButton("Off##cm", &cm, 0)) cam->cullMode = ModuleCamera::CullMode::None;
                ImGui::SameLine();
                if (ImGui::RadioButton("Frustum##cm", &cm, 1)) cam->cullMode = ModuleCamera::CullMode::Frustum;
                int cs = (int)cam->cullSource;
                ImGui::Text("Cull From"); ImGui::SameLine();
                if (ImGui::RadioButton("Editor##cs", &cs, 0)) cam->cullSource = ModuleCamera::CullSource::EditorCamera;
                ImGui::SameLine();
                if (ImGui::RadioButton("Game##cs", &cs, 1)) cam->cullSource = ModuleCamera::CullSource::GameCamera;
                ImGui::Separator();
                ImGui::Text("Debug Draw");
                ImGui::MenuItem("Editor Frustum", nullptr, &cam->debugDrawEditorFrustum);
                ImGui::MenuItem("Cull Frustum", nullptr, &cam->debugDrawCullFrustum);
                ImGui::MenuItem("Camera Axes", nullptr, &cam->debugDrawCameraAxes);
                ImGui::MenuItem("Forward Ray", nullptr, &cam->debugDrawForwardRay);
                ImGui::Separator();
                ImGui::Text("Camera Parameters");
                float fovDeg = cam->fovY * 57.2957795f;
                ImGui::SetNextItemWidth(140.f);
                if (ImGui::SliderFloat("FOV##cam", &fovDeg, 10.f, 170.f)) cam->fovY = fovDeg * 0.0174532925f;
                ImGui::SetNextItemWidth(140.f);
                ImGui::DragFloat("Near##cam", &cam->nearZ, 0.01f, 0.01f, 10.f);
                ImGui::SetNextItemWidth(140.f);
                ImGui::DragFloat("Far##cam", &cam->farZ, 1.f, 10.f, 5000.f);
                ImGui::SetNextItemWidth(140.f);
                ImGui::SliderFloat("Aspect##cam", &cam->aspectRatio, 0.5f, 4.f);
                ImGui::Separator();
                Vector3 fwd = cam->getForward();
                ImGui::TextDisabled("Pos: %.2f  %.2f  %.2f", cam->getPos().x, cam->getPos().y, cam->getPos().z);
                ImGui::TextDisabled("Fwd: %.2f  %.2f  %.2f", fwd.x, fwd.y, fwd.z);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")){
        for (EditorPanel* p : m_panels){
            const char* n = p->getName();
            if (strcmp(n,"Render Graph")==0 || strcmp(n,"GPU Memory")==0 ||
                strcmp(n,"Collision Debug")==0 || strcmp(n,"Performance")==0) continue;
            ImGui::MenuItem(n, nullptr, &p->open);
        }
        ImGui::Separator();
        ImGui::SeparatorText("PROFILING");
        for (EditorPanel* p : m_panels){
            const char* n = p->getName();
            if (strcmp(n,"Render Graph")==0 || strcmp(n,"GPU Memory")==0 ||
                strcmp(n,"Collision Debug")==0 || strcmp(n,"Performance")==0)
                ImGui::MenuItem(n, nullptr, &p->open);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) m_firstFrame = true;
        ImGui::EndMenu();
    }

    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        char gpuInfo[128];
        snprintf(gpuInfo, sizeof(gpuInfo), "RTX \xC2\xB7 build Development    %.0f fps",
            (double)app->getFPS());
        float textW = ImGui::CalcTextSize(gpuInfo).x;
        float rightX = ImGui::GetWindowWidth() - textW - 14.f;
        if (rightX > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(rightX);
        ImGui::TextUnformatted(gpuInfo);
        ImGui::PopStyleColor();
    }

    ImGui::EndMainMenuBar();
}

void ModuleEditor::drawStatusBar(){
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kStatusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 3.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.048f, 0.048f, 0.060f, 1.f));
    constexpr ImGuiWindowFlags kF =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("##StatusBar", nullptr, kF)){
        ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(3); return;
    }

    ImGui::PushFont(g_fontMono);

    {
        ImVec2 dp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(dp.x + 5.f, dp.y + 8.f), 4.f,
            ImGui::ColorConvertFloat4ToU32(EditorColors::Ok));
        ImGui::Dummy(ImVec2(14.f, 0.f)); ImGui::SameLine(0, 0);

        const char* sceneName = "Untitled";
        static char sceneNameBuf[128];
        if (m_sceneManager && !m_currentScenePath.empty()){
            snprintf(sceneNameBuf, sizeof(sceneNameBuf), "%s",
                std::filesystem::path(m_currentScenePath).filename().string().c_str());
            sceneName = sceneNameBuf;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
        ImGui::Text("Ready  \xC2\xB7  Scene: %s", sceneName);
        ImGui::PopStyleColor();
        if (m_selection.has()){
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
            ImGui::Text("  \xC2\xB7  1 selected  \xC2\xB7  ");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
            ImGui::TextUnformatted(m_selection.object->getName().c_str());
            ImGui::PopStyleColor();
        }
    }

    {
        ModuleD3D12* d3d = app->getD3D12();
        ModuleStaticBuffer* sb = app->getStaticBuffer();
        SIZE_T vramBytes = d3d ? d3d->getDedicatedVideoMemory() : 0;
        float vramUsed = sb ? (float)sb->getUsedBytes() / (1024.f*1024.f*1024.f) : 0.f;
        float vramTotal = vramBytes > 0
            ? (float)vramBytes / (1024.f*1024.f*1024.f)
            : (sb ? (float)sb->getTotalBytes() / (1024.f*1024.f*1024.f) : 0.f);

        char main[192], vram[48];
        snprintf(main, sizeof(main),
            "Renderer: D3D12  \xC2\xB7  Passes: 9  \xC2\xB7  Draw Calls: %d  \xC2\xB7  ",
            m_frameDrawCalls);
        if (vramTotal > 0.f) snprintf(vram, sizeof(vram), "VRAM: %.2f / %.1f GB", vramUsed, vramTotal);
        else snprintf(vram, sizeof(vram), "VRAM: —");

        float mainW = ImGui::CalcTextSize(main).x;
        float vramW = ImGui::CalcTextSize(vram).x;
        float totalW = mainW + vramW + 4.f;

        ImVec2 wp = ImGui::GetWindowPos();
        float wW = ImGui::GetWindowWidth();
        float textY = wp.y + (kStatusH - ImGui::GetTextLineHeight()) * 0.5f;
        float rx = wp.x + wW - totalW - 8.f;

        ImDrawList* ddl = ImGui::GetWindowDrawList();
        ddl->AddText(ImVec2(rx, textY),
            ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2), main);
        ddl->AddText(ImVec2(rx + mainW + 4.f, textY),
            ImGui::ColorConvertFloat4ToU32(EditorColors::Acc), vram);
    }

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void ModuleEditor::handleDialogs(){
    auto tryScene = [&](bool ok, const char* good, const char* bad){ log(ok ? good : bad, ok ? EditorColors::Success : EditorColors::Danger); };
    if (m_saveDialog->draw() && m_sceneManager->getActiveScene()){
        const std::string& p = m_saveDialog->getSelectedPath();
        if (m_sceneManager->saveCurrentScene(p)){
            m_currentScenePath = p;
            m_savePointIndex = (int)m_undoStack.size();
            m_redoStack.clear();
            tryScene(true, "Scene saved!", "");
        }
        else tryScene(false, "", "Failed to save scene.");
    }
    if (m_loadDialog->draw() && m_sceneManager->getActiveScene()){
        const std::string& p = m_loadDialog->getSelectedPath();
        if (m_sceneManager->loadScene(p)){ m_currentScenePath = p; tryScene(true, "Scene loaded!", ""); }
        else tryScene(false, "", "Failed to load scene.");
    }
}

void ModuleEditor::handleNewScenePopup(ID3D12GraphicsCommandList*){
    if (m_showNewSceneConfirm){ ImGui::OpenPopup("New Scene?"); m_showNewSceneConfirm = false; }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("New Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("This will clear the current scene.");
    ImGui::TextColored(EditorColors::Warning, "Unsaved changes will be lost!");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("Create New Scene", ImVec2(160, 0))){
        app->getD3D12()->flush();
        m_sceneManager->setScene(std::make_unique<EmptyScene>(), app->getD3D12()->getDevice());
        setupDefaultScene();
        m_selection.clear();
        m_currentScenePath.clear();
        m_undoStack.clear();
        m_redoStack.clear();
        m_savePointIndex = 0;
        log("New scene created (top-down camera + skybox).", EditorColors::Success);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ModuleEditor::handleShortcuts(){
    if (ImGui::GetIO().WantTextInput) return;
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_N, false)) m_showNewSceneConfirm = true;
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_N, false)) createEmptyGameObject();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false)){
        if (!m_currentScenePath.empty() && m_sceneManager->getActiveScene()){
            bool ok = m_sceneManager->saveCurrentScene(m_currentScenePath);
            if (ok){ m_savePointIndex = (int)m_undoStack.size(); m_redoStack.clear(); }
            log(ok ? "Scene saved!" : "Failed to save.", ok ? EditorColors::Success : EditorColors::Danger);
        }
        else m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false)) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && m_selection.has()) deleteGameObject(m_selection.object);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && m_sceneManager && m_sceneManager->isEditingPrefab()){ exitPrefabEdit(); return; }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undoToSavePoint();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelected();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_V, false)) pasteClipboard();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelected();

    if (!ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_P, false)){
        static int spawnIdx = 0;
        ++spawnIdx;
        static const PrimitiveType kTypes[] = {
            PrimitiveType::Cube, PrimitiveType::Sphere,
            PrimitiveType::Capsule, PrimitiveType::Cylinder
        };
        PrimitiveType t = kTypes[spawnIdx % 4];
        float x = static_cast<float>((spawnIdx * 3) % 11 - 5);
        float z = static_cast<float>((spawnIdx * 7) % 11 - 5);
        spawnPrimitive(t, Vector3(x, 5.f, z), Vector3::One, true);
    }
}

static void drawDashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b,
                            ImU32 col, float thick, float dash, float gap){
    auto seg = [&](ImVec2 p0, ImVec2 p1){
        float dx = p1.x - p0.x, dy = p1.y - p0.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) return;
        float nx = dx / len, ny = dy / len, t = 0.f;
        bool on = true;
        while (t < len){
            float end = (std::min)(t + (on ? dash : gap), len);
            if (on)
                dl->AddLine({ p0.x + nx * t, p0.y + ny * t },
                             { p0.x + nx * end, p0.y + ny * end },
                             col, thick);
            t = end;
            on = !on;
        }
    };
    seg({ a.x, a.y }, { b.x, a.y });
    seg({ b.x, a.y }, { b.x, b.y });
    seg({ b.x, b.y }, { a.x, b.y });
    seg({ a.x, b.y }, { a.x, a.y });
}

void ModuleEditor::drawDragDropOverlay(){
    auto& ddm = DragDropManager::Get();
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    const ImVec2 dsz = ImGui::GetIO().DisplaySize;

    if (ddm.IsDragging()){
        fg->AddRectFilled({ 0.f, 0.f }, dsz, IM_COL32(0, 0, 0, 100));

        constexpr float bW = 440.f, bH = 130.f;
        const ImVec2 bMin{ dsz.x * 0.5f - bW * 0.5f, dsz.y * 0.5f - bH * 0.5f };
        const ImVec2 bMax{ dsz.x * 0.5f + bW * 0.5f, dsz.y * 0.5f + bH * 0.5f };

        fg->AddRectFilled(bMin, bMax, IM_COL32(18, 20, 30, 225), 10.f);
        drawDashedRect(fg, bMin, bMax, IM_COL32(80, 160, 255, 220), 2.f, 14.f, 6.f);

        const char* kLine1 = "Drop Assets to Import";
        const char* kLine2 = ".gltf  .glb  .fbx  .obj  .png  .jpg  .tga  .dds  .hdr";
        const ImVec2 s1 = ImGui::CalcTextSize(kLine1);
        const ImVec2 s2 = ImGui::CalcTextSize(kLine2);
        const float cx = (bMin.x + bMax.x) * 0.5f;
        const float cy = (bMin.y + bMax.y) * 0.5f;

        fg->AddText({ cx - s1.x * 0.5f, cy - s1.y - 6.f },
                    IM_COL32(210, 225, 255, 255), kLine1);
        fg->AddText({ cx - s2.x * 0.5f, cy + 6.f },
                    IM_COL32(120, 145, 185, 200), kLine2);
    }

    const DragDropManager::ImportProgress prog = ddm.GetProgress();
    if (prog.active || prog.showComplete){
        constexpr float kW = 500.f, kH = 200.f;
        ImGui::SetNextWindowPos(
            { dsz.x * 0.5f - kW * 0.5f, dsz.y * 0.5f - kH * 0.5f },
            ImGuiCond_Always);
        ImGui::SetNextWindowSize({ kW, kH }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.93f);

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs;

        if (ImGui::Begin("##DDProgressModal", nullptr, kFlags)){
            if (prog.active){
                std::string fname = prog.currentFile;
                if (fname.size() > 60)
                    fname = "\xe2\x80\xa6" + fname.substr(fname.size() - 59);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.92f, 1.0f, 1.f));
                ImGui::SetWindowFontScale(1.25f);
                ImGui::TextUnformatted(fname.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::Spacing();

                char subtitle[64];
                snprintf(subtitle, sizeof(subtitle),
                         "Importing %d of %d file%s",
                         prog.current + 1, prog.total,
                         prog.total == 1 ? "" : "s");
                ImGui::TextColored({ 0.65f, 0.72f, 0.82f, 1.f }, "%s", subtitle);

                ImGui::Spacing();

                const float pct = (prog.total > 0)
                    ? static_cast<float>(prog.current) / static_cast<float>(prog.total)
                    : 0.f;
                ImGui::ProgressBar(pct, { -1.f, 0.f });

            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.92f, 0.45f, 1.f));
                ImGui::SetWindowFontScale(1.25f);
                char done[80];
                snprintf(done, sizeof(done), "Import complete  (%d file%s)",
                         prog.total, prog.total == 1 ? "" : "s");
                ImGui::TextUnformatted(done);
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::ProgressBar(1.f, { -1.f, 0.f });
            }

            if (!prog.completedFiles.empty()){
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::BeginChild("##DDDoneList", { 0.f, 0.f }, false,
                                  ImGuiWindowFlags_NoScrollbar);
                for (const auto& cf : prog.completedFiles){
                    ImGui::TextColored({ 0.45f, 0.88f, 0.52f, 1.f },
                                       "\xe2\x9c\x93 %s", cf.c_str());
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}
