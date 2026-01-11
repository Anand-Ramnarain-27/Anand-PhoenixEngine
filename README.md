# Anand-PhoenixEngine

A modern, modular C++ game engine built with DirectX 12, featuring real-time rendering, an extensible editor, and a clean architecture. Developed as part of academic and independent study in game engine programming.

**GitHub Repository:** [https://github.com/Anand-Ramnarain-27/Anand-PhoenixEngine](https://github.com/Anand-Ramnarain-27/Anand-PhoenixEngine)

---

## 🚀 Overview

PhoenixEngine is a custom-built game engine designed to showcase core engine systems including rendering, camera control, resource management, and editor tooling. It implements modern graphics techniques and a modular design that supports extensibility and maintainability.

The engine is developed with performance and clarity in mind, using modern C++ standards, DirectX 12 for graphics, and Dear ImGui for editor interfaces.

---

## 🛠️ Features

### 🎨 Rendering
- DirectX 12 backend with triple buffering
- Phong shading model support
- Real-time texture filtering (Bilinear / Point, Wrap / Clamp)
- Automatic mipmap generation
- Debug visualization (grid, axes, gizmos)
- Aspect ratio preservation on window resize

### 🎮 Camera System
- Unity-style camera controls
- Orbit, FPS movement, zoom, and focus modes
- Smooth input handling with speed modifiers

### 🧰 Editor & Tools
- ImGui-based editor interface
- Real-time performance monitoring (FPS graph, stats)
- Gizmo system for object manipulation (translate, rotate, scale)
- Material and light parameter editing in real-time
- Scene view rendered to texture in editor window

### 📦 Architecture
- Modular system design (Renderer, Camera, ResourceManager, Editor)
- STL containers with const-correctness and RAII principles
- No memory leaks, clean resource lifecycle management
- Shader compilation to .cso with hot-reload support

### 📁 Asset Pipeline
- Support for DDS, PNG, GLTF formats
- Runtime texture processing and mipmap generation
- Structured asset folder layout

---

## 📂 Repository Structure

```

## 🧪 Projects Built with PhoenixEngine

### Assignment 1 – 3D Texture Viewer
- Renders a textured quad with mipmapped textures
- Unity-like camera controls
- Editor toggles for grid, axes, and texture filtering modes

### Assignment 2 – PBR Model Viewer & Editor
- Loads and renders GLTF models with Phong shading
- Interactive gizmos for transform edits
- Real-time material and lighting controls

---

## 🖥️ System Requirements

- **OS:** Windows 10/11 64-bit
- **GPU:** DirectX 12 compatible
- **CPU:** x64 architecture
- **RAM:** 4 GB minimum
- **Build Tool:** Visual Studio 2022

---

## 📦 How to Build

1. Clone the repository:
   ```bash
   git clone https://github.com/Anand-Ramnarain-27/Anand-PhoenixEngine.git
   ```
2. Open `PhoenixEngine.sln` in Visual Studio 2022
3. Build in **Release** mode
4. Run `PhoenixEngine.exe` from the output folder

---

## 🎮 Controls

| Action | Input |
|--------|--------|
| Look around | Right-click + drag |
| Move camera | Right-click + WASD |
| Zoom | Mouse wheel |
| Orbit | Alt + left-click + drag |
| Focus on object | F |
| Speed boost | Hold Shift |

---

## 📄 License

This project is licensed under the **MIT License**. See [LICENCE.md](LICENCE.md) for details.

Third-party libraries are included with their respective licenses in the `Licences/` folder.

---

## 👨‍💻 Author

**Anand Ramnarain**  
Game Engine Programming Student  

---
