# Anand-PhoenixEngine

## Overview
PhoenixEngine | Game Engine Development Portfolio A C++ game engine built for academic assignments, featuring modern graphics APIs, real-time rendering, and modular architecture. PhoenixEngine is a custom DirectX 12 game engine developed for educational purposes. This first assignment implements a complete 3D quad and texture viewer with Unity-like camera controls and a fully-featured editor interface.

## Repository
GitHub Repository: https://github.com/Anand-Ramnarain-27/Anand-PhoenixEngine  

## System Requirements
- **OS**: Windows 10/11 (64-bit)
- **Graphics**: DirectX 12 compatible GPU
- **Memory**: 4GB RAM minimum
- **Storage**: 30MB free space
- **Dependencies**: Visual C++ Redistributable (included)

## How to Run
1. Extract the downloaded `PhoenixEngine_v0.1.zip`
2. Navigate to the `PhoenixEngine` folder
3. Double-click `PhoenixEngine.exe`
4. The application will start with a textured 3D quad at the origin

## Controls

### Camera Controls (Unity-style)
- **Right Click + Mouse Drag**: Free look around
- **Right Click + WASD**: FPS-style movement
- **Mouse Wheel**: Zoom in/out
- **Alt + Left Click + Drag**: Orbit around the textured quad
- **F Key**: Focus camera on the 3D quad
- **Shift Key**: Hold to double movement speed

### Editor Interface
The engine features a comprehensive ImGui-based editor with multiple windows:

#### Main Menu Bar
- Access all editor windows
- Real-time FPS counter display

#### Texture Viewer Options Window
- **Show Grid**: Toggle the ground plane grid (XYZ orientation)
- **Show Axis**: Toggle the XYZ coordinate axes at origin
- **Texture Filter**: Choose from 4 filtering modes:
  - Wrap + Bilinear (Default)
  - Wrap + Point
  - Clamp + Bilinear
  - Clamp + Point

#### Additional Windows
- **FPS Graph**: Real-time performance monitoring with statistics
- **About**: Engine information and module details
- **Controls**: This help reference

## Features Beyond Requirements

### Enhanced Editor Tools
- **Performance Monitoring**: Detailed FPS graphs with min/max/avg statistics
- **Professional UI**: Collapsible windows, proper layout, and intuitive controls

### Engine Architecture
- **Module System**: Clean separation of concerns (Renderer, Camera, Editor, etc.)
- **Resource Management**: Automatic texture loading with fallback system
- **Debug Visualization**: Integrated debug drawing for grids and axes

### Technical Features
- **Mipmap Generation**: Automatic mipmap creation for textures lacking them
- **Aspect Ratio Preservation**: Graphics maintain proportions when window is resized
- **Fullscreen Support**: Toggle with proper mode switching
- **Triple Buffering**: Smooth rendering with 3-frame flight architecture

## Texture Support
- **Formats**: DDS (with mipmaps), PNG
- **Automatic Mipmaps**: If texture lacks mipmaps, they're generated at runtime
- **Filtering**: All 4 required combinations implemented
- **Fallback**: Checkerboard pattern if texture fails to load

## Project Structure
```
PhoenixEngine/
├── PhoenixEngine.exe        # Main executable
├── Assets/                  # Resources folder
│   └── Textures/
│       └── dog.dds          # Example texture
├── *.dll                    # Required libraries
├── *.cso                    # Compiled shaders
└── [Other runtime files]
```

## Third-Party Libraries
- **DirectX 12**: Graphics API
- **Dear ImGui**: Immediate mode GUI
- **DirectXTex**: Texture loading and processing
- **DirectX Tool Kit**: Math utilities and helpers
- **Debug Draw**: 3D debug visualization


## Known Issues
- None - all assignment requirements fully implemented

## Submission Information
- **Student**: Anand Ramnarain
- **Course**: Game Engines
- **Date**: December 2025
- **Build Size**: 25MB

## License
This project is licensed under the MIT License - see the [LICENCE.md](LICENCE.md) file for details.

---

### Testing Verification
All assignment requirements have been tested:
- ✅ Camera controls respond correctly to all inputs
- ✅ Texture filtering modes work as expected
- ✅ Mipmaps visible when zooming with bilinear filtering
- ✅ Window resize maintains aspect ratio
- ✅ Grid and axis toggles work immediately
- ✅ Release build runs without crashes


