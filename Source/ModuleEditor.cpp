#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "GraphicsSamplers.h"
#include "ModuleTextureSampler.h"
#include "ModuleModelViewer.h"
#include "ModuleCamera.h"
#include "ImGuiPass.h"

#include <algorithm>
#include <float.h>

ModuleEditor::ModuleEditor()
{
    fps_log.reserve(FPS_HISTORY_SIZE);
    ms_log.reserve(FPS_HISTORY_SIZE);
}

ModuleEditor::~ModuleEditor()
{
}

bool ModuleEditor::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = 1;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    auto device = d3d12->getDevice();
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiHeap))))
    {
        return false;
    }

    imguiPass = std::make_unique<ImGuiPass>(
        d3d12->getDevice(),
        d3d12->getHWnd(),
        imguiHeap->GetCPUDescriptorHandleForHeapStart(),
        imguiHeap->GetGPUDescriptorHandleForHeapStart()
    );

    logBuffer.push_back("ModuleEditor initialized");
    logBuffer.push_back("Texture filtering: Wrap + Bilinear");

    return imguiPass != nullptr;
}

bool ModuleEditor::cleanUp()
{
    imguiPass.reset();

    if (imguiHeap)
    {
        imguiHeap->Release();
        imguiHeap = nullptr;
    }

    app->getD3D12()->flush();

    return true;
}

void ModuleEditor::preRender()
{
    if (imguiPass)
    {
        imguiPass->startFrame();

        ModuleD3D12* d3d12 = app->getD3D12();
        if (d3d12)
        {
            unsigned width = d3d12->getWindowWidth();
            unsigned height = d3d12->getWindowHeight();
            ImGuizmo::BeginFrame();
            ImGuizmo::SetRect(0, 0, float(width), float(height));
        }

        imGuiDrawCommands();
    }
}

void ModuleEditor::render()
{
    if (!imguiPass) return;

    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleTextureSampler* textureSampler = app->getTextureSampler();
	ModuleModelViewer* modelViewer = app->getModelViewer();

    ID3D12GraphicsCommandList* commandList = d3d12->beginFrameRender();

    d3d12->setBackBufferRenderTarget(Vector4(0.188f, 0.208f, 0.259f, 1.0f));

    if (!commandList) return;

    if (textureSampler)
    {
        textureSampler->render3DContent(commandList);
    }

    if (modelViewer)
    {
        modelViewer->render3DContent(commandList);
    }

    ID3D12DescriptorHeap* heaps[] = { imguiHeap };
    commandList->SetDescriptorHeaps(1, heaps);

    imguiPass->record(commandList);

    d3d12->endFrameRender();
}

void ModuleEditor::imGuiDrawCommands()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Texture Viewer Options", nullptr, &showTextureWindow);
            ImGui::MenuItem("Geometry Viewer Options", nullptr, &showModelViewerWindow);
            ImGui::MenuItem("Console", nullptr, &showConsole);
            ImGui::MenuItem("FPS Graph", nullptr, &showFPS);
            ImGui::MenuItem("About", nullptr, &showAbout);
            ImGui::MenuItem("Controls", nullptr, &showControls);
            ImGui::EndMenu();
        }

        float currentFPS = ImGui::GetIO().Framerate;
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);
        ImGui::Text("FPS: %.1f", currentFPS);

        ImGui::EndMainMenuBar();
    }

    if (showTextureWindow)
    {
        ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_FirstUseEver);
        ImGui::Begin("Texture Viewer Options", &showTextureWindow);

        ImGui::Checkbox("Show Grid", &showGrid);
        ImGui::Checkbox("Show Axis", &showAxis);

        ImGui::Separator();

        const char* filterItems[] = {
            "Wrap + Bilinear",
            "Wrap + Point",
            "Clamp + Bilinear",
            "Clamp + Point"
        };

        int currentIdx = static_cast<int>(currentFilter);
        if (ImGui::Combo("Texture Filter", &currentIdx, filterItems, IM_ARRAYSIZE(filterItems)))
        {
            currentFilter = static_cast<GraphicsSamplers::Type>(currentIdx);
            logBuffer.push_back(std::string("Texture filter: ") + filterItems[currentIdx]);
        }

        ImGui::End();
    }

    if (showModelViewerWindow)
    {
        ModuleModelViewer* modelViewer = app->getModelViewer();
        ModuleCamera* camera = app->getCamera();
        if (modelViewer)
        {
            ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Geometry Viewer Options", &showModelViewerWindow))
            {
                bool modelShowGrid = modelViewer->IsGridVisible();
                bool modelShowAxis = modelViewer->IsAxisVisible();
                bool modelShowGuizmo = modelViewer->IsGuizmoVisible();
                ImGuizmo::OPERATION currentGizmoOp = modelViewer->GetGizmoOperation();

                if (ImGui::Checkbox("Show grid", &modelShowGrid))
                    modelViewer->SetShowGrid(modelShowGrid);

                if (ImGui::Checkbox("Show axis", &modelShowAxis))
                    modelViewer->SetShowAxis(modelShowAxis);

                if (ImGui::Checkbox("Show guizmo", &modelShowGuizmo))
                    modelViewer->SetShowGuizmo(modelShowGuizmo);

                ImGui::Separator();

                if (ImGui::RadioButton("Translate", (int*)&currentGizmoOp, (int)ImGuizmo::TRANSLATE))
                    modelViewer->SetGizmoOperation(currentGizmoOp);
                ImGui::SameLine();

                if (ImGui::RadioButton("Rotate", (int*)&currentGizmoOp, ImGuizmo::ROTATE))
                    modelViewer->SetGizmoOperation(currentGizmoOp);

                ImGui::SameLine();
                if (ImGui::RadioButton("Scale", (int*)&currentGizmoOp, ImGuizmo::SCALE))
                    modelViewer->SetGizmoOperation(currentGizmoOp);

                if (modelViewer->HasModel())
                {
                    auto& model = modelViewer->GetModel();

                    ImGui::Text("Model: %s", model->getSrcFile().c_str());
                    ImGui::Text("Meshes: %zu", model->getMeshes().size());
                    ImGui::Text("Materials: %zu", model->getMaterials().size());

                    ImGui::Separator();

                    Matrix modelMatrix = modelViewer->GetModelMatrix();

                    float translation[3], rotation[3], scale[3];
                    ImGuizmo::DecomposeMatrixToComponents(
                        (float*)&modelMatrix,
                        translation,
                        rotation,
                        scale
                    );

                    bool changed = false;
                    changed |= ImGui::DragFloat3("Position", translation, 0.1f);
                    changed |= ImGui::DragFloat3("Rotation", rotation, 0.1f);
                    changed |= ImGui::DragFloat3("Scale", scale, 0.1f);

                    if (changed)
                    {
                        ImGuizmo::RecomposeMatrixFromComponents(
                            translation,
                            rotation,
                            scale,
                            (float*)&modelMatrix
                        );
                        modelViewer->SetModelMatrix(modelMatrix);
                    }

                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Material Editor", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        const auto& materials = model->getMaterials();

                        static int selectedMaterial = 0;
                        if (materials.size() > 1)
                        {
                            std::vector<std::string> materialNames;
                            for (const auto& mat : materials)
                            {
                                materialNames.push_back(mat->getName().empty()
                                    ? "Material " + std::to_string(materialNames.size())
                                    : mat->getName());
                            }

                            std::vector<const char*> materialNamesCStr;
                            for (const auto& name : materialNames)
                            {
                                materialNamesCStr.push_back(name.c_str());
                            }

                            ImGui::Combo("Select Material", &selectedMaterial,
                                materialNamesCStr.data(), (int)materialNamesCStr.size());
                        }

                        if (selectedMaterial < materials.size())
                        {
                            auto* material = materials[selectedMaterial].get();

                            Material::PhongMaterial currentPhongMat = material->getPhong();
                            bool materialChanged = false;

                            ImGui::Separator();
                            ImGui::Text("Editing: %s", material->getName().c_str());

                            float diffuseColor[3] = {
                                currentPhongMat.diffuseColor.x,
                                currentPhongMat.diffuseColor.y,
                                currentPhongMat.diffuseColor.z
                            };

                            if (ImGui::ColorEdit3("Diffuse Color", diffuseColor))
                            {
                                currentPhongMat.diffuseColor = XMFLOAT4(diffuseColor[0], diffuseColor[1], diffuseColor[2], 1.0f);
                                materialChanged = true;
                            }

                            if (ImGui::DragFloat("Diffuse Strength (Kd)", &currentPhongMat.Kd, 0.01f, 0.0f, 1.0f))
                            {
                                materialChanged = true;
                            }

                            if (ImGui::DragFloat("Specular Strength (Ks)", &currentPhongMat.Ks, 0.01f, 0.0f, 1.0f))
                            {
                                materialChanged = true;
                            }

                            if (ImGui::DragFloat("Shininess", &currentPhongMat.shininess, 1.0f, 1.0f, 512.0f))
                            {
                                materialChanged = true;
                            }

                            if (materialChanged)
                            {
                                material->setPhong(currentPhongMat);
                                logBuffer.push_back("Updated material: " + material->getName());
                            }

                            ImGui::Separator();
                            ImGui::Text("Texture:");
                            ImGui::SameLine();

                            if (material->hasTexture())
                            {
                                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Loaded");
                            }
                            else
                            {
                                ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "None");
                            }

                            ImGui::Separator();
                            ImGui::Text("Material Stats:");
                            ImGui::Text("  Kd: %.2f", currentPhongMat.Kd);
                            ImGui::Text("  Ks: %.2f", currentPhongMat.Ks);
                            ImGui::Text("  Shininess: %.0f", currentPhongMat.shininess);
                            ImGui::Text("  Has Texture: %s", material->hasTexture() ? "Yes" : "No");

                            if (ImGui::Button("Reset Coefficients"))
                            {
                                Material::PhongMaterial current = material->getPhong();

                                current.Kd = 0.8f;
                                current.Ks = 0.2f;
                                current.shininess = 32.f;

                                material->setPhong(current);
                                logBuffer.push_back("Reset coefficients for material: " + material->getName());
                            }
                        }
                        else if (!materials.empty())
                        {
                            ImGui::Text("No material selected");
                        }
                    }
                }
                else
                {
                    ImGui::Text("No model loaded");
                }
                ImGui::Separator();

                if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto light = modelViewer->GetLight();

                    if (ImGui::DragFloat3("Light Direction", (float*)&light.L, 0.1f, -1.0f, 1.0f))
                    {
                        light.L.Normalize();
                        modelViewer->SetLight(light);
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Normalize"))
                    {
                        light.L.Normalize();
                        modelViewer->SetLight(light);
                    }

                    if (ImGui::ColorEdit3("Light Colour", (float*)&light.Lc))
                        modelViewer->SetLight(light);

                    if (ImGui::ColorEdit3("Ambient Colour", (float*)&light.Ac))
                        modelViewer->SetLight(light);
                }
                ImGuiIO& io = ImGui::GetIO();
                if (camera)
                {
                    camera->setEnable(
                        !io.WantCaptureMouse &&
                        !ImGuizmo::IsUsing()
                    );
                }
            }
            ImGui::End();
        }
    }

    if (showConsole)
    {
        if (ImGui::Begin("Console", &showConsole))
        {
            if (ImGui::Button("Clear"))
            {
                logBuffer.clear();
            }
            ImGui::SameLine();

            ImGui::Separator();
            for (const auto& log : logBuffer)
            {
                ImGui::Text("%s", log.c_str());
            }
        }
        ImGui::End();
    }

    if (showFPS)
    {
        update_counter++;

        if (update_counter >= UPDATE_INTERVAL)
        {
            update_counter = 0;

            float currentFPS = ImGui::GetIO().Framerate;
            float currentMS = 1000.0f / currentFPS;

            fps_log.push_back(currentFPS);
            ms_log.push_back(currentMS);

            if (fps_log.size() > FPS_HISTORY_SIZE)
            {
                fps_log.erase(fps_log.begin());
                ms_log.erase(ms_log.begin());
            }
        }

        if (ImGui::Begin("FPS Graph", &showFPS))
        {
            float currentFPS = ImGui::GetIO().Framerate;
            float currentMS = 1000.0f / currentFPS;

            ImGui::Text("Current FPS: %.1f", currentFPS);
            ImGui::SameLine();
            ImGui::Text("Frame Time: %.2f ms", currentMS);

            ImGui::Separator();

            if (!fps_log.empty())
            {
                char title[25];

                sprintf_s(title, 25, "Framerate %.1f", fps_log.back());
                ImGui::PlotHistogram("##framerate",
                    fps_log.data(),
                    static_cast<int>(fps_log.size()),
                    0,
                    title,
                    0.0f,
                    300.0f,
                    ImVec2(310, 100));

                sprintf_s(title, 25, "Milliseconds %0.1f", ms_log.back());
                ImGui::PlotHistogram("##milliseconds",
                    ms_log.data(),
                    static_cast<int>(ms_log.size()),
                    0,
                    title,
                    0.0f,
                    10.0f,
                    ImVec2(310, 100));

                if (ImGui::CollapsingHeader("Statistics"))
                {
                    float minFPS = *std::min_element(fps_log.begin(), fps_log.end());
                    float maxFPS = *std::max_element(fps_log.begin(), fps_log.end());
                    float sumFPS = 0;
                    for (float fps : fps_log) sumFPS += fps;
                    float avgFPS = sumFPS / fps_log.size();

                    ImGui::Text("Min FPS: %.1f", minFPS);
                    ImGui::Text("Max FPS: %.1f", maxFPS);
                    ImGui::Text("Avg FPS: %.1f", avgFPS);
                    ImGui::Text("Samples: %d", (int)fps_log.size());
                }
            }
            else
            {
                ImGui::Text("Collecting data... (%d frames remaining)", UPDATE_INTERVAL - update_counter);
            }
        }
        ImGui::End();
    }

    if (showAbout)
    {
        if (ImGui::Begin("About", &showAbout))
        {
            ImGui::Text("PhoenixEngine v0.1");
            ImGui::Separator();

            ImGui::Text("A DirectX 12 Game Engine");
            ImGui::Text("Built for learning and development");

            ImGui::Separator();
            ImGui::Text("Renderer:");
            ImGui::BulletText("DirectX 12 Graphics API");
            ImGui::BulletText("Texture & Mesh Systems");
            ImGui::BulletText("Shader Management");

            ImGui::Separator();
            ImGui::Text("Editor:");
            ImGui::BulletText("ImGui Interface");
            ImGui::BulletText("Performance Monitoring");
            ImGui::BulletText("Scene Configuration");

            ImGui::Separator();
            ImGui::Text("Modules:");
            ImGui::BulletText("D3D12 Renderer");
            ImGui::BulletText("Resource Manager");
            ImGui::BulletText("Editor UI");
            ImGui::BulletText("Graphics Samplers");
        }
        ImGui::End();
    }

    if (showControls)
    {
        ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Controls", &showControls))
        {
            ImGui::Text("Editor Controls");
            ImGui::Separator();

            ImGui::Text("Windows:");
            ImGui::BulletText("Main: Scene controls");
            ImGui::BulletText("Controls: This window");
            ImGui::BulletText("FPS Graph: Performance");
            ImGui::BulletText("Console: Log output");
            ImGui::BulletText("About: Engine info");

            ImGui::Separator();
            ImGui::Text("Scene Visualization:");
            ImGui::BulletText("Show Grid: Toggle ground plane");
            ImGui::BulletText("Show Axis: Toggle XYZ axes");

            ImGui::Separator();
            ImGui::Text("Texture Settings:");
            ImGui::BulletText("Filtering: 4 texture modes");
            ImGui::BulletText("Wrap/Clamp: Address modes");
            ImGui::BulletText("Bilinear/Point: Filter types");

            ImGui::Separator();
            ImGui::Text("Camera Controls:");
            ImGui::BulletText("Right Click + Mouse: Free look");
            ImGui::BulletText("Right Click + WASD: FPS movement");
            ImGui::BulletText("Mouse Wheel: Zoom in/out");
            ImGui::BulletText("Alt + Left Click: Orbit object");
            ImGui::BulletText("F: Focus camera on geometry");
            ImGui::BulletText("Shift: Double movement speed");

            ImGui::Separator();
            ImGui::Text("Performance:");
            ImGui::BulletText("FPS Counter: Real-time display");
            ImGui::BulletText("Frame Time: Per-frame timing");
            ImGui::BulletText("Graphs: Visual performance trends");
        }
        ImGui::End();
    }
}