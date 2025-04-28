#pragma once

#include "base_renderpass.hpp"

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vector>

#include "vulkan_types.hpp"

struct SphereSSBO {
	alignas(16) glm::vec3 center;
	alignas(16) glm::vec3 color;
	float radius;
};

struct AABB {
	glm::vec3 min;
	glm::vec3 max;
};

class RayTracingPipeline {
public:
	RayTracingPipeline (
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		CommonDescriptor& commonDescriptor,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		const Descriptor& outputDescriptor
	) : physicalDevice(physicalDevice),
		device(device),
		commonDescriptor(commonDescriptor),
		commandPool(commandPool),
		graphicsQueue(graphicsQueue),
		outputDescriptor(outputDescriptor) {};
	~RayTracingPipeline() = default;
	void init();
	void cleanup();
	void render(
		const VkCommandBuffer& commandBuffer,
		uint32_t imageIndex,
		uint32_t currentFrame,
		const Camera& camera,
		const std::vector<DirectionalLightBuffer>& directionalLights,
		VkExtent2D extent
	);

private:
    void loadEXTFunctions();
	void createSpheres();
    void getRayTracingProperties();
    void createBLAS();
	void createTLAS();
	void createSBT();
	void createSphereSSBO();
	void createDescriptorPool();
	void createDescriptor();
	void createSampler();
	void createPipeline();
	inline uint32_t alignup(uint32_t s, uint32_t alignment) {
		return (s + alignment - 1) & ~(alignment - 1);
	}

	std::vector<SphereSSBO> spheres;
	// std::vector<SphereSSBO> spheres{
	// 	SphereSSBO{glm::vec3(0.0f, -1000.0f, 0.0f), glm::vec3(0.8f, 0.8f, 0.0f), 1000.0f},
	// 	SphereSSBO{glm::vec3(-6.0f, 1.0f, -5.0f), glm::vec3(0.8f, 0.8f, 0.8f), 1.0f},
	// 	SphereSSBO{glm::vec3(6.0f, 2.0f, -5.0f), glm::vec3(1.0f, 1.0f, 1.0f), 2.0f},
	// 	SphereSSBO{glm::vec3(0.0f, 3.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f), 3.0f},
	// 	SphereSSBO{glm::vec3(10.0f, 2.0f, -2.0f), glm::vec3(0.8f, 0.6f, 0.2f), 2.0f}
	// };

	VkPhysicalDevice physicalDevice;
	VkDevice device;
	CommonDescriptor& commonDescriptor;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR property{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};

	BufferResource aabbBufferResource;
	BufferResource blasBufferResource;
	BufferResource instanceBufferResource;
	BufferResource tlasBufferResource;
	BufferResource sbtBufferResource;
	BufferResource sphereBufferResource;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	Descriptor tlasDescriptor;
	Descriptor sphereDescriptor;
	const Descriptor& outputDescriptor;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
	VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
	VkDeviceAddress tlasDeviceAddress = 0;

	VkCommandPool commandPool;
	VkQueue graphicsQueue;

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
	VkStridedDeviceAddressRegionKHR raygenSBT{};
	VkStridedDeviceAddressRegionKHR missSBT{};
	VkStridedDeviceAddressRegionKHR hitSBT{};
	VkStridedDeviceAddressRegionKHR callableSBT{};

	// declare RT EXT function pointers as member variables to avoid redeclaration problem between current Vulkan headers
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
};