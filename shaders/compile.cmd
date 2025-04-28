@echo off
glslc ./screen_quad.vert -o screen_quad_vert.spv
glslc ./swapchain.frag -o swapchain_frag.spv
glslc ./forward.vert -o forward_vert.spv
glslc ./forward.frag -o forward_frag.spv
glslc ./deferred_gbuffer.vert -o deferred_gbuffer_vert.spv
glslc ./deferred_gbuffer.frag -o deferred_gbuffer_frag.spv
glslc ./deferred_lighting.frag -o deferred_lighting_frag.spv
glslc ./ssao.frag -o ssao_frag.spv
glslc ./shadowmap.vert -o shadowmap_vert.spv
glslc ./shadowmap.frag -o shadowmap_frag.spv
glslc ./pixel.frag -o pixel_frag.spv
glslc ./rtow.rgen --target-env=vulkan1.3 -o rtow_rgen.spv
glslc ./rtow.rmiss --target-env=vulkan1.3 -o rtow_rmiss.spv
glslc ./rtow.rint --target-env=vulkan1.3 -o rtow_rint.spv
glslc ./rtow_diffuse.rchit --target-env=vulkan1.3 -o rtow_diffuse_rchit.spv
glslc ./rtow_metal.rchit --target-env=vulkan1.3 -o rtow_metal_rchit.spv
glslc ./rtow_dielectric.rchit --target-env=vulkan1.3 -o rtow_dielectric_rchit.spv