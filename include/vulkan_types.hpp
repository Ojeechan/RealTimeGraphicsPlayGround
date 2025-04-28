#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <cstddef>
#include <cstring>
#include <array>
#include <vector>

#include "game_object.hpp"
#include "buffer_types.hpp"

struct VertexBufferResource {
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
};

struct ImageResource {
	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;

	void cleanup(VkDevice device) {
		vkDestroyImageView(device, imageView, nullptr);
		vkDestroyImage(device, image, nullptr);
		vkFreeMemory(device, imageMemory, nullptr);
	}
};

struct ModelResource {
	VertexBufferResource vertexBufferResource;
	VertexBufferResource indexBufferResource;
	std::array<ImageResource, 3> textureResources;
	std::vector<VkDescriptorSet> descriptorSets;
	size_t indexCount;

	void cleanup(VkDevice device) {
		for (auto textureResource : textureResources) {
			textureResource.cleanup(device);
		}

		vkDestroyBuffer(device, indexBufferResource.buffer, nullptr);
		vkFreeMemory(device, indexBufferResource.bufferMemory, nullptr);

		vkDestroyBuffer(device, vertexBufferResource.buffer, nullptr);
		vkFreeMemory(device, vertexBufferResource.bufferMemory, nullptr);
	}
};

struct Descriptor {
	VkDescriptorSetLayout layout;
	std::vector<VkDescriptorSet> sets;

	void cleanup(VkDevice device) {
		vkDestroyDescriptorSetLayout(device, layout, nullptr);
	}

	void resize(size_t size) {
		sets.resize(size);
	}
};

struct CommonDescriptor {
	Descriptor modelMatrix;
	Descriptor cameraMatrix;
	Descriptor camera;
	Descriptor light;

	void cleanup(VkDevice device) {
		modelMatrix.cleanup(device);
		cameraMatrix.cleanup(device);
		camera.cleanup(device);
		light.cleanup(device);
	}
};

struct BufferResource {
	std::vector<VkBuffer> buffers;
	std::vector<VkDeviceMemory> buffersMemory;
	std::vector<void*> buffersMapped;

	void cleanup(VkDevice device) {
		for (int i = 0; i < buffers.size(); ++i) {
			vkDestroyBuffer(device, buffers[i], nullptr);
		}
		for (int i = 0; i < buffersMemory.size(); ++i) {
			vkFreeMemory(device, buffersMemory[i], nullptr);
		}
	}

	void resize(size_t size) {
		buffers.resize(size);
		buffersMemory.resize(size);
		buffersMapped.resize(size);
	}
};

struct AssetData {
	GameObject object;
	ModelResource resource;

	uint32_t updateModelTransformMatrix(uint32_t index, void* modelMatrixBufferMapped) const {
		uint32_t offset = static_cast<uint32_t>(index * sizeof(TransformMatrixBuffer));
		TransformMatrixBuffer matrixUBO{};
		matrixUBO.model = object.getModelMatrix();
		void* target = static_cast<char*>(modelMatrixBufferMapped) + offset;
		memcpy(target, &matrixUBO, sizeof(matrixUBO));
		return offset;
	}
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	inline bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Swapchain {
	VkSwapchainKHR handle = VK_NULL_HANDLE;
	uint32_t minImageCount;
	VkFormat imageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {0, 0};
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
};