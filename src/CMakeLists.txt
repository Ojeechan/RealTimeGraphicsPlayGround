add_executable(RTGraphicsApp "main.cpp")
target_sources(RTGraphicsApp PRIVATE
    "rt_graphics_app.cpp"
    "vulkan_state.cpp"
    "window_state.cpp"
    "update_system.cpp"
    "graphics_system.cpp"
    "vulkan_utils.cpp"
    "forward_renderpass.cpp"
    "deferred_renderpass.cpp"
    "pixel_renderpass.cpp"
    "gbuffer_renderpass.cpp"
    "shadowmapping_renderpass.cpp"
    "gui_renderpass.cpp"
    "raytracing_pipeline.cpp"
    "swapchain_renderpass.cpp"
    "${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_impl_vulkan.cpp"
	"${CMAKE_SOURCE_DIR}/external/imgui/imgui_impl_glfw.cpp"
)

target_include_directories(RTGraphicsApp PRIVATE "${CMAKE_SOURCE_DIR}/include" PRIVATE "${CMAKE_SOURCE_DIR}/external")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
target_link_libraries(RTGraphicsApp
    glfw
    Vulkan::Vulkan
)

if (WIN32)
    target_compile_definitions(RTGraphicsApp PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX)
    target_compile_definitions(RTGraphicsApp PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif()