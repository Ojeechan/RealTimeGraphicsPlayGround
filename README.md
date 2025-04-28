# About This Project
This project is a playground to experiment with various graphics techniques freely.

The program is scaffolded from some tutorials:
- Vulkan Tutorial: https://vulkan-tutorial.com/
- NVIDIA Vulkan Ray Tracing Tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR
- Ray Tracing in One Weekend: https://raytracing.github.io/books/RayTracingInOneWeekend.html

# Explored Topics
- Vulkan
- Vulkan Ray Tracing
- C++
- ImGUI
- Crude Asset Loader (a simple, rough one)
- Forward Rendering
- Deferred Rendering
- Shadow Mapping
- Pixelize Shader
- Real-Time Ray Tracing in One Weekend

# Things To Explore
- Continuous Improvements
	- Shadow
	- Asset Management System
	- PBR
	- Ray Tracing Optimization
	- Rendering Pipeline Architecture
- Graphics API Abstraction (OpenGL, DirectX)
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

## Install GLFW
GLFW is required.

If not already installed, get it using your preferred method.

https://www.glfw.org/download

## Build
### Windows (using VisualStudio 2022)
1. Generate build configuration
```
cd VulkanRenderer
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="path/to/glfw"
```
2. Launch VulkanRenderer/build/VulkanRenderer.sln
3. Build project `VulkanRenderer` (only Release builds are available for now)

### Linux (using default generator)
1. Generate build configuration
```
cd VulkanRenderer
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
Execute VulkanRenderer/shaders/compile.cmd

### Linux
Execute VulkanRenderer/shaders/compile.sh

## Run Program
### Windows
```
cd VulkanRender/bin
VulkanRenderer.exe
```
### Linux
```
cd VulkanRender/bin
./VulkanRenderer
```
# Licenses

This project uses the following third-party libraries, each of which has its own license:

- **imgui**: MIT License ([licenses/imgui/LICENSE](licenses/imgui/LICENSE.txt))
- **nlohmann/json**: MIT License ([Licenses/nlohmann_json/LICENSE](licenses/nlohmann_json/LICENSE.MIT))
- **stb_image**: MIT License ([Licenses/stb_image/LICENSE](licenses/stb_image/LICENSE))
- **tiny_obj**: MIT License ([Licenses/stb_image/LICENSE](licenses/tiny_obj/LICENSE))