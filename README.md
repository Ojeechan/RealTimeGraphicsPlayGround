([Supplementary document in Japanese/日本語の補足資料](README_ja.md))

# About This Project
This project is a playground to experiment with various real-time graphics techniques freely.

The program is scaffolded from some tutorials:
- Vulkan Tutorial: https://vulkan-tutorial.com/
- NVIDIA Vulkan Ray Tracing Tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR
- Ray Tracing in One Weekend: https://raytracing.github.io/books/RayTracingInOneWeekend.html

# Entry Point
The application starts at [src/main.cpp](https://github.com/Ojeechan/RealTimeGraphicsPlayground/blob/develop/src/main.cpp).

If you're interested in the code, feel free to reference from there.

# Explored Topics
- Vulkan
- Vulkan Ray Tracing
- C++
- ImGUI
- Crude Asset Loader (a simple, rough one)
- Dynamic Pipeline Switching
- Forward Rendering
- Deferred Rendering
- Shadow Mapping
- Pixelize Shader
- Real-Time Ray Tracing in One Weekend

## Scene Examples
### Forward Rendering
![Forward Rendering](docs/images/forward.png)

### Deferred Rendering and Shadow Mapping
![Deferred Rendering and Shadow Mapping](docs/images/deferred_shadowmapping.png)

### Pixelize Shader
![Pixelize Shader](docs/images/pixelize.png)

### Real-Time Ray Tracing in One Weekend
![Real-Time Ray Tracing in One Weekend](docs/images/realtime_rtow.png)

# Things To Explore
- Continuous Improvements
	- Shadow
	- Asset Management System
	- PBR
	- Ray Tracing Optimization
	- Rendering Pipeline Architecture
- Graphics API Abstraction (OpenGL, DirectX)
- Hybrid Rendering (Rasterization and Ray Tracing)
- Jittered Rendering
- IBR
- Sky, Weather Rendering
- GI
- Alpha Blending
- Post Effects
	- DoF
	- SSAO
	- Scanline/CRT Effect
- Skeletal Animation
- Camera Control System
- Light Control System
	- Multi-Lighting
- LOD, Tessellation, Mipmaps
- Collision Detection
- Physics
- Compute Shader
- Parallel Computing (CUDA, OpenCL)
- Producer-Consumer Pattern (for Update and Graphics Subsystems)
- USD Conversion (between Edit and Runtime Modes)
- Build Automation on GitHub Actions
- Data-Oriented Programming
- Nihonga (日本画) Shader

# How To Setup

## Install Vulkan and GLFW
Vulkan API 1.3 and GLFW are required.
(Recent versions of the Vulkan SDK include GLM.)

If not already installed, get it using your preferred method.

Refer to the following for setup instructions:
https://vulkan-tutorial.com/Development_environment

## Clone Repository
```
git clone https://github.com/Ojeechan/RealTimeGraphicsPlayground.git
```

## Build
### Windows (using VisualStudio 2022)
1. Generate build configuration
```
cd RealTimeGraphicsPlayground
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="path/to/glfw"
```
2. Launch RealTimeGraphicsPlayground/build/RealTimeGraphicsPlayground.sln
3. Build project `RealTimeGraphicsPlayground` (only Release builds are available for now)

### Linux (using default generator)
1. Generate build configuration
```
cd RealTimeGraphicsPlayground
mkdir build
cd build
cmake ..
```
2. Build
```
cmake --build .
```

## Compile Shaders
### Windows
Execute `RealTimeGraphicsPlayground/shaders/compile.cmd`

### Linux
Execute `RealTimeGraphicsPlayground/shaders/compile.sh`

## Run Program
### Windows
```
cd RealTimeGraphicsPlayground/bin
RTGraphicsApp.exe
```
### Linux
```
cd RealTimeGraphicsPlayground/bin
./RTGraphicsApp
```
# Licenses

This project uses the following third-party libraries, each of which has its own license:

- **imgui**: MIT License ([licenses/imgui/LICENSE](licenses/imgui/LICENSE.txt))
- **nlohmann_json**: MIT License ([Licenses/nlohmann_json/LICENSE](licenses/nlohmann_json/LICENSE.MIT))
- **stb_image**: MIT License ([Licenses/stb_image/LICENSE](licenses/stb_image/LICENSE))
- **tiny_obj**: MIT License ([Licenses/stb_image/LICENSE](licenses/tiny_obj/LICENSE))