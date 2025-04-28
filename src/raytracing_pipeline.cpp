#include "raytracing_pipeline.hpp"

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <random>

#include "vulkan_utils.hpp"
#include "constants.hpp"

struct PushConstants{
	glm::vec2 windowSize;
	float seed;
};

void RayTracingPipeline::init() {
	loadEXTFunctions();
	getRayTracingProperties();
	createSpheres();
	createBLAS();
	createTLAS();
	createSphereSSBO();
	createDescriptorPool();
	createDescriptor();
	createPipeline();
	createSBT();
}

// define RT EXT function pointers here to avoid redeclaration problem between current Vulkan headers
void RayTracingPipeline::loadEXTFunctions() {
	vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
	vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
	vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
	vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
	vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
	vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
	vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR) vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
}

void RayTracingPipeline::cleanup() {
	aabbBufferResource.cleanup(device);
	blasBufferResource.cleanup(device);
	instanceBufferResource.cleanup(device);
	tlasBufferResource.cleanup(device);
	sbtBufferResource.cleanup(device);
	sphereBufferResource.cleanup(device);
	vkDestroyAccelerationStructureKHR(device, tlas, nullptr);
	vkDestroyAccelerationStructureKHR(device, blas, nullptr);
	tlasDescriptor.cleanup(device);
	sphereDescriptor.cleanup(device);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

void RayTracingPipeline::render(
	const VkCommandBuffer& commandBuffer,
	uint32_t imageIndex,
	uint32_t currentFrame,
	const Camera& camera,
	const std::vector<DirectionalLightBuffer>& directionalLights,
	VkExtent2D extent
) {
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		pipelineLayout,
		0,
		1,
		&tlasDescriptor.sets[0],
		0,
		nullptr
	);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		pipelineLayout,
		1,
		1,
		&outputDescriptor.sets[currentFrame],
		0,
		nullptr
	);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		pipelineLayout,
		2,
		1,
		&commonDescriptor.cameraMatrix.sets[currentFrame],
		0,
		nullptr
	);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		pipelineLayout,
		3,
		1,
		&commonDescriptor.camera.sets[currentFrame],
		0,
		nullptr
	);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		pipelineLayout,
		4,
		1,
		&sphereDescriptor.sets[0],
		0,
		nullptr
	);

	PushConstants pushConstants{};
	pushConstants.windowSize = glm::vec2(extent.width, extent.height);
	pushConstants.seed = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

	vkCmdPushConstants(
		commandBuffer,
		pipelineLayout,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0,
		sizeof(PushConstants),
		&pushConstants
	);

	vkCmdTraceRaysKHR(
		commandBuffer,
		&raygenSBT,
		&missSBT,
		&hitSBT,
		&callableSBT,
		extent.width,
		extent.height,
		1
	);
}

void RayTracingPipeline::getRayTracingProperties() {
	VkPhysicalDeviceProperties2 deviceProps2 = {};
	deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProps2.pNext = &property;

	vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);
}

void RayTracingPipeline::createSpheres() {
    spheres.push_back(SphereSSBO{glm::vec3(0.0f, -1000.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.5f), 1000.0f});
	std::random_device rd;
    std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(0.0f, 1.0f);
	for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            glm::vec3 position(a + 0.9f * dis(gen), 0.2f, b + 0.9f * dis(gen));

            if ((position - glm::vec3(4.0f, 0.2f, 0.0f)).length() > 0.9f) {
				spheres.push_back(SphereSSBO{position, glm::vec3(dis(gen),  dis(gen),  dis(gen)), 0.2f});
			}
        }
    }
}

void RayTracingPipeline::createBLAS() {
	AABB aabb{};
	aabb.min = glm::vec3(-0.5f);
	aabb.max = glm::vec3(0.5f);

	VkDeviceSize aabbBufferSize = sizeof(AABB);

	VkMemoryAllocateFlagsInfo allocFlagsInfo{};
	allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	allocFlagsInfo.pNext = nullptr;

	aabbBufferResource.resize(1);
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		aabbBufferSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		aabbBufferResource.buffers[0],
		aabbBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

	vkMapMemory(device, aabbBufferResource.buffersMemory[0], 0, aabbBufferSize, 0, &aabbBufferResource.buffersMapped[0]);
	memcpy(aabbBufferResource.buffersMapped[0], &aabb, static_cast<size_t>(aabbBufferSize));
	vkUnmapMemory(device, aabbBufferResource.buffersMemory[0]);

	VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
	bufferDeviceAddressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddressInfo.buffer = aabbBufferResource.buffers[0];

	VkDeviceAddress aabbBufferDeviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);

	VkAccelerationStructureGeometryKHR blasGeom{};
	blasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	blasGeom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
	blasGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	blasGeom.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
	blasGeom.geometry.aabbs.data.deviceAddress = aabbBufferDeviceAddress;
	blasGeom.geometry.aabbs.stride = sizeof(AABB);

	VkAccelerationStructureBuildRangeInfoKHR blasBuildRangeInfo{};
	blasBuildRangeInfo.primitiveCount  = 1;
	blasBuildRangeInfo.primitiveOffset = 0;
	blasBuildRangeInfo.firstVertex     = 0;
	blasBuildRangeInfo.transformOffset = 0;

	VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeomInfo{};
	blasBuildGeomInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	blasBuildGeomInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	blasBuildGeomInfo.flags  = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	blasBuildGeomInfo.mode   = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	blasBuildGeomInfo.geometryCount = 1;
	blasBuildGeomInfo.pGeometries   = &blasGeom;

	VkAccelerationStructureBuildSizesInfoKHR blasBuildSizesInfo{};
	blasBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&blasBuildGeomInfo,
		&blasBuildRangeInfo.primitiveCount,
		&blasBuildSizesInfo
	);

	blasBufferResource.resize(1);
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		blasBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		blasBufferResource.buffers[0],
		blasBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

	BufferResource scratchBufferResource{};
	scratchBufferResource.resize(1);
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		blasBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		scratchBufferResource.buffers[0],
		scratchBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

	VkBufferDeviceAddressInfo scratchAddressInfo{};
	scratchAddressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	scratchAddressInfo.buffer = scratchBufferResource.buffers[0];

	VkDeviceAddress scratchBufferDeviceAddress = vkGetBufferDeviceAddress(device, &scratchAddressInfo);

	VkAccelerationStructureCreateInfoKHR asCreateInfo{};
	asCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asCreateInfo.buffer = blasBufferResource.buffers[0];
	asCreateInfo.offset = 0;
	asCreateInfo.size   = blasBuildSizesInfo.accelerationStructureSize;
	asCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	if (vkCreateAccelerationStructureKHR(device, &asCreateInfo, nullptr, &blas) != VK_SUCCESS) {
		throw std::runtime_error("failed to create BLAS");
	}

	blasBuildGeomInfo.dstAccelerationStructure = blas;
	blasBuildGeomInfo.scratchData.deviceAddress = scratchBufferDeviceAddress;
	const VkAccelerationStructureBuildRangeInfoKHR* pASBuildRangeInfo = &blasBuildRangeInfo;
	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);
	{
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &blasBuildGeomInfo, &pASBuildRangeInfo);
	} VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);

	scratchBufferResource.cleanup(device);

}

void RayTracingPipeline::createTLAS() {
    VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo{};
    blasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    blasAddressInfo.accelerationStructure = blas;
    VkDeviceAddress blasDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &blasAddressInfo);

    std::vector<VkAccelerationStructureInstanceKHR> instances;

	instances.resize(spheres.size());
	for (uint32_t i = 0; i < instances.size(); i++) {
		glm::vec3 translate = spheres[i].center;
		glm::vec3 scale = glm::vec3(spheres[i].radius * 2.0f);
		VkTransformMatrixKHR transform{{
			{ scale.x, 0.0f,   0.0f,   translate.x },
			{ 0.0f,   scale.y, 0.0f,   translate.y },
			{ 0.0f,   0.0f,   scale.z, translate.z }
		}};

		instances[i].transform = transform;
		instances[i].instanceCustomIndex = i;
		instances[i].mask = 0xFF;
		instances[i].instanceShaderBindingTableRecordOffset = i % 3;
		instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instances[i].accelerationStructureReference = blasDeviceAddress;
	}

    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
	instanceBufferResource.resize(1);
	VkMemoryAllocateFlagsInfo allocFlagsInfo{};
	allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	allocFlagsInfo.pNext = nullptr;

	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		instanceBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		instanceBufferResource.buffers[0],
        instanceBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

    vkMapMemory(device, instanceBufferResource.buffersMemory[0], 0, instanceBufferSize, 0, &instanceBufferResource.buffersMapped[0]);
	memcpy(instanceBufferResource.buffersMapped[0], instances.data(), static_cast<size_t>(instanceBufferSize));

	VkBufferDeviceAddressInfo instanceBufferAddressInfo{};
    instanceBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    instanceBufferAddressInfo.buffer = instanceBufferResource.buffers[0];
    VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(device, &instanceBufferAddressInfo);

    VkAccelerationStructureGeometryInstancesDataKHR tlasGeomInstancesData{};
    tlasGeomInstancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeomInstancesData.arrayOfPointers = VK_FALSE;
    tlasGeomInstancesData.data.deviceAddress = instanceBufferAddress;

    VkAccelerationStructureGeometryKHR tlasGeom{};
    tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances = tlasGeomInstancesData;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeomInfo{};
    tlasBuildGeomInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuildGeomInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuildGeomInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuildGeomInfo.geometryCount = 1;
    tlasBuildGeomInfo.pGeometries = &tlasGeom;

    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo{};
    tlasBuildRangeInfo.primitiveCount = static_cast<uint32_t>(instances.size());
    tlasBuildRangeInfo.primitiveOffset = 0;
    tlasBuildRangeInfo.firstVertex = 0;
    tlasBuildRangeInfo.transformOffset = 0;

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo{};
    tlasBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
		device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildGeomInfo,
        &tlasBuildRangeInfo.primitiveCount,
        &tlasBuildSizesInfo
	);

	tlasBufferResource.resize(1);
    VulkanUtils::createBuffer(
		physicalDevice,
		device,
        tlasBuildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBufferResource.buffers[0],
        tlasBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
    tlasCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreateInfo.buffer = tlasBufferResource.buffers[0];
    tlasCreateInfo.offset = 0;
    tlasCreateInfo.size   = tlasBuildSizesInfo.accelerationStructureSize;
    tlasCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    if(vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, nullptr, &tlas) != VK_SUCCESS) {
		throw std::runtime_error("failed to create TLAS");
	}

	BufferResource scratchBufferResource{};
	scratchBufferResource.resize(1);
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		tlasBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		scratchBufferResource.buffers[0],
		scratchBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

	VkBufferDeviceAddressInfo scratchBufferAddressInfo{};
    scratchBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchBufferAddressInfo.buffer = scratchBufferResource.buffers[0];
    VkDeviceAddress scratchBufferAddress = vkGetBufferDeviceAddress(device, &scratchBufferAddressInfo);

    tlasBuildGeomInfo.dstAccelerationStructure = tlas;
    tlasBuildGeomInfo.scratchData.deviceAddress = scratchBufferAddress;

	const VkAccelerationStructureBuildRangeInfoKHR* pTLASBuildRangeInfo = &tlasBuildRangeInfo;

	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);
	{
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlasBuildGeomInfo, &pTLASBuildRangeInfo);
	} VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);

    VkAccelerationStructureDeviceAddressInfoKHR tlasAddressInfo{};
    tlasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    tlasAddressInfo.accelerationStructure = tlas;
    tlasDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &tlasAddressInfo);

    scratchBufferResource.cleanup(device);
}

void RayTracingPipeline::createSBT() {
	// referenced: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/#raytracingdescriptorset
	uint32_t missCount{1};
	uint32_t hitCount{3};
	auto handleCount = 1 + missCount + hitCount;
	uint32_t handleSize = property.shaderGroupHandleSize;
	uint32_t handleSizeAligned = alignup(handleSize, property.shaderGroupHandleAlignment);
	raygenSBT.stride = alignup(handleSizeAligned, property.shaderGroupBaseAlignment);
	raygenSBT.size = raygenSBT.stride;
	missSBT.stride = handleSizeAligned;
	missSBT.size = alignup(missCount * handleSizeAligned, property.shaderGroupBaseAlignment);
	hitSBT.stride = handleSizeAligned;
	hitSBT.size = alignup(hitCount * handleSizeAligned, property.shaderGroupBaseAlignment);

	uint32_t dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	auto result = vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, handleCount, dataSize, handles.data());
	assert(result == VK_SUCCESS);

	VkDeviceSize sbtSize = raygenSBT.size + missSBT.size + hitSBT.size + callableSBT.size;

	VkMemoryAllocateFlagsInfo allocFlagsInfo{};
	allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	allocFlagsInfo.pNext = nullptr;

	sbtBufferResource.resize(1);
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		sbtSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		sbtBufferResource.buffers[0],
		sbtBufferResource.buffersMemory[0],
		&allocFlagsInfo
	);

	VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbtBufferResource.buffers[0]};
	VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device, &info);
	raygenSBT.deviceAddress = sbtAddress;
	missSBT.deviceAddress = sbtAddress + raygenSBT.size;
	hitSBT.deviceAddress = sbtAddress + raygenSBT.size + missSBT.size;

	auto getHandle = [&] (int i) { return handles.data() + i * handleSize; };

	vkMapMemory(device, sbtBufferResource.buffersMemory[0], 0, sbtSize, 0, &sbtBufferResource.buffersMapped[0]);
	auto*    pSBTBuffer = reinterpret_cast<uint8_t*>(sbtBufferResource.buffersMapped[0]);
	uint8_t* pData{nullptr};
	uint32_t handleIdx{0};

	pData = pSBTBuffer;
	memcpy(pData, getHandle(handleIdx++), handleSize);

	pData = pSBTBuffer + raygenSBT.size;
	for(uint32_t c = 0; c < missCount; c++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += missSBT.stride;
	}

	pData = pSBTBuffer + raygenSBT.size + missSBT.size;
	for(uint32_t c = 0; c < hitCount; c++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += hitSBT.stride;
	}

	vkUnmapMemory(device, sbtBufferResource.buffersMemory[0]);
}

void RayTracingPipeline::createSphereSSBO() {
	sphereBufferResource.resize(1);
	VkDeviceSize size = sizeof(SphereSSBO) * spheres.size();
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sphereBufferResource.buffers[0],
		sphereBufferResource.buffersMemory[0],
		nullptr
	);
	vkMapMemory(device, sphereBufferResource.buffersMemory[0], 0, size, 0, &sphereBufferResource.buffersMapped[0]);
	memcpy(sphereBufferResource.buffersMapped[0], spheres.data(), static_cast<size_t>(size));
}

void RayTracingPipeline::createDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1 + 1;

	if(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ray tracing descriptor pool");
	}
}

void RayTracingPipeline::createDescriptor() {
	// descritor for TLAS
	VkDescriptorSetLayoutBinding tlasLayoutBinding{};
	tlasLayoutBinding.binding = 0;
	tlasLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlasLayoutBinding.descriptorCount = 1;
	tlasLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	tlasLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo tlasLayoutInfo{};
	tlasLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	tlasLayoutInfo.bindingCount = 1;
	tlasLayoutInfo.pBindings = &tlasLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &tlasLayoutInfo, nullptr, &tlasDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create TLAS descriptor set layout");
	}

	VkDescriptorSetAllocateInfo tlasAllocInfo{};
	tlasAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	tlasAllocInfo.descriptorPool = descriptorPool;
	tlasAllocInfo.descriptorSetCount = 1;
	tlasAllocInfo.pSetLayouts = &tlasDescriptor.layout;

	tlasDescriptor.resize(1);
	if(vkAllocateDescriptorSets(device, &tlasAllocInfo, tlasDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate TLAS descriptor set");
	}

	VkWriteDescriptorSetAccelerationStructureKHR asWrite = {};
	asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asWrite.accelerationStructureCount = 1;
	asWrite.pAccelerationStructures = &tlas;

	VkWriteDescriptorSet asDescriptorSetWrite = {};
	asDescriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	asDescriptorSetWrite.dstSet = tlasDescriptor.sets[0];
	asDescriptorSetWrite.dstBinding = 0;
	asDescriptorSetWrite.dstArrayElement = 0;
	asDescriptorSetWrite.descriptorCount = 1;
	asDescriptorSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	asDescriptorSetWrite.pNext = &asWrite;

	vkUpdateDescriptorSets(device, 1, &asDescriptorSetWrite, 0, nullptr);

	// descriptor for Sphere SSBO
	VkDescriptorSetLayoutBinding sphereLayoutBinding{};
	sphereLayoutBinding.binding = 0;
	sphereLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	sphereLayoutBinding.descriptorCount = 1;
	sphereLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	sphereLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo sphereLayoutInfo{};
	sphereLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	sphereLayoutInfo.bindingCount = 1;
	sphereLayoutInfo.pBindings = &sphereLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &sphereLayoutInfo, nullptr, &sphereDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create Sphere descriptor set layout");
	}

	VkDescriptorSetAllocateInfo sphereAllocInfo{};
	sphereAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	sphereAllocInfo.descriptorPool = descriptorPool;
	sphereAllocInfo.descriptorSetCount = 1;
	sphereAllocInfo.pSetLayouts = &sphereDescriptor.layout;

	sphereDescriptor.resize(1);
	if(vkAllocateDescriptorSets(device, &sphereAllocInfo, sphereDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate Sphere descriptor set");
	}

	VkDescriptorBufferInfo sphereBufferInfo{};
	sphereBufferInfo.buffer = sphereBufferResource.buffers[0];
	sphereBufferInfo.offset = 0;
	sphereBufferInfo.range = sizeof(SphereSSBO) * spheres.size();

	VkWriteDescriptorSet sphereDescriptorSetWrite = {};
	sphereDescriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	sphereDescriptorSetWrite.dstSet = sphereDescriptor.sets[0];
	sphereDescriptorSetWrite.dstBinding = 0;
	sphereDescriptorSetWrite.dstArrayElement = 0;
	sphereDescriptorSetWrite.descriptorCount = 1;
	sphereDescriptorSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	sphereDescriptorSetWrite.pBufferInfo = &sphereBufferInfo;
	sphereDescriptorSetWrite.pNext = nullptr;

	vkUpdateDescriptorSets(device, 1, &sphereDescriptorSetWrite, 0, nullptr);
}

void RayTracingPipeline::createPipeline() {
	std::array<VkDescriptorSetLayout, 5> descriptorSetLayouts{
		tlasDescriptor.layout,
		outputDescriptor.layout,
		commonDescriptor.cameraMatrix.layout,
		commonDescriptor.camera.layout,
		sphereDescriptor.layout
	};
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ray tracing pipeline layout");
	}

	auto rgenCode = VulkanUtils::readFile("../shaders/rtow_rgen.spv");
	auto rmissCode = VulkanUtils::readFile("../shaders/rtow_rmiss.spv");
	auto rintCode = VulkanUtils::readFile("../shaders/rtow_rint.spv");
	auto rchitDiffuseCode = VulkanUtils::readFile("../shaders/rtow_diffuse_rchit.spv");
	auto rchitMetalCode = VulkanUtils::readFile("../shaders/rtow_metal_rchit.spv");
	auto rchitDielectricCode = VulkanUtils::readFile("../shaders/rtow_dielectric_rchit.spv");

	VkShaderModule rgenModule = VulkanUtils::createShaderModule(device, rgenCode);
	VkShaderModule rmissModule = VulkanUtils::createShaderModule(device, rmissCode);
	VkShaderModule rintModule = VulkanUtils::createShaderModule(device, rintCode);
	VkShaderModule rchitDiffuseModule = VulkanUtils::createShaderModule(device, rchitDiffuseCode);
	VkShaderModule rchitMetalModule = VulkanUtils::createShaderModule(device, rchitMetalCode);
	VkShaderModule rchitDielectricModule = VulkanUtils::createShaderModule(device, rchitDielectricCode);

	VkPipelineShaderStageCreateInfo rgenStage{};
	rgenStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rgenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rgenStage.module = rgenModule;
	rgenStage.pName = "main";

	VkPipelineShaderStageCreateInfo rmissStage{};
	rmissStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rmissStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	rmissStage.module = rmissModule;
	rmissStage.pName = "main";

	VkPipelineShaderStageCreateInfo rintStage{};
	rintStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rintStage.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	rintStage.module = rintModule;
	rintStage.pName = "main";

	VkPipelineShaderStageCreateInfo rchitDiffuseStage{};
	rchitDiffuseStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rchitDiffuseStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	rchitDiffuseStage.module = rchitDiffuseModule;
	rchitDiffuseStage.pName = "main";

	VkPipelineShaderStageCreateInfo rchitMetalStage{};
	rchitMetalStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rchitMetalStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	rchitMetalStage.module = rchitMetalModule;
	rchitMetalStage.pName = "main";

	VkPipelineShaderStageCreateInfo rchitDielectricStage{};
	rchitDielectricStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rchitDielectricStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	rchitDielectricStage.module = rchitDielectricModule;
	rchitDielectricStage.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 6> shaderStages{
		rgenStage,
		rmissStage,
		rintStage,
		rchitDiffuseStage,
		rchitMetalStage,
		rchitDielectricStage
	};

	shaderGroups.resize(5);
	shaderGroups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[0].pNext = nullptr;
	shaderGroups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[0].generalShader = 0;
	shaderGroups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

	shaderGroups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[1].pNext = nullptr;
	shaderGroups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[1].generalShader = 1;
	shaderGroups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

	shaderGroups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[2].pNext = nullptr;
	shaderGroups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	shaderGroups[2].generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[2].closestHitShader = 3;
	shaderGroups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[2].intersectionShader = 2;

	shaderGroups[3].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[3].pNext = nullptr;
	shaderGroups[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	shaderGroups[3].generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[3].closestHitShader = 4;
	shaderGroups[3].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[3].intersectionShader = 2;

	shaderGroups[4].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[4].pNext = nullptr;
	shaderGroups[4].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	shaderGroups[4].generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[4].closestHitShader = 5;
	shaderGroups[4].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[4].intersectionShader = 2;

	VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
	pipelineInfo.pGroups = shaderGroups.data();
	pipelineInfo.maxPipelineRayRecursionDepth = 31;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ray tracing pipeline");
	}

	vkDestroyShaderModule(device, rgenModule, nullptr);
	vkDestroyShaderModule(device, rmissModule, nullptr);
	vkDestroyShaderModule(device, rintModule, nullptr);
	vkDestroyShaderModule(device, rchitDiffuseModule, nullptr);
	vkDestroyShaderModule(device, rchitMetalModule, nullptr);
	vkDestroyShaderModule(device, rchitDielectricModule, nullptr);
}
