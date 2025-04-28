#include "deferred_renderpass.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <cstddef>
#include <array>
#include <stdexcept>
#include <vector>

#include "vulkan_vertex.hpp"
#include "vulkan_utils.hpp"
#include "vulkan_types.hpp"
#include "gui_renderpass.hpp"
#include "buffer_types.hpp"
#include "constants.hpp"
#include "shadowmapping_renderpass.hpp"

enum BINDING {
	ALBEDO = 0,
	POSITION = 1,
	NORMAL = 2,
	MATERIAL = 3,
	DEPTH = 4,
	SSAO = 5
};

struct SSAOPushConstant{
	glm::vec2 screenSize;
};

void DeferredRenderPass::init() {
	shadowPass->init();
	createRenderPass();
	createImageResources();
	createGraphicsPipeline();
}

void DeferredRenderPass::cleanup() {
	shadowPass->cleanup();
	cleanupImageResources();
	vkDestroyPipeline(device, gBufferPipeline, nullptr);
	vkDestroyPipeline(device, ssaoPipeline, nullptr);
	vkDestroyPipeline(device, lightingPipeline, nullptr);
	vkDestroyPipelineLayout(device, gBufferPipelineLayout, nullptr);
	vkDestroyPipelineLayout(device, ssaoPipelineLayout, nullptr);
	vkDestroyPipelineLayout(device, lightingPipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
}

void DeferredRenderPass::createImageResources(){
	createGBuffers();
	createFramebuffers();
	createDescriptorSetLayout();
	createDescriptorPool();
	createDescriptorSets();
}

void DeferredRenderPass::cleanupImageResources() {
	albedo.cleanup(device);
	position.cleanup(device);
	normal.cleanup(device);
	material.cleanup(device);
	depth.cleanup(device);
	ssao.cleanup(device);

	for (auto framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	ssaoDescriptor.cleanup(device);
	lightingDescriptor.cleanup(device);

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}


void DeferredRenderPass::createRenderPass() {
	VkAttachmentDescription albedoAttachment{};
	albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
	albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription positionAttachment{};
	positionAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	positionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	positionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	positionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	positionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	positionAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription normalAttachment{};
	normalAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	normalAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription materialAttachment{};
	materialAttachment.format = VK_FORMAT_R8G8_UNORM;
	materialAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	materialAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	materialAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	materialAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	materialAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription ssaoAttachment{};
	ssaoAttachment.format = VK_FORMAT_R8_UNORM;
	ssaoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	ssaoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ssaoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ssaoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ssaoAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription swapchainAttachment{};
	swapchainAttachment.format = swapchain.imageFormat;
	swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapchainAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// G-Buffer subpass settings
	std::array<VkAttachmentReference, 4> gBufferOutputReferences;
	gBufferOutputReferences[0] = {BINDING::ALBEDO, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	gBufferOutputReferences[1] = {BINDING::POSITION, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	gBufferOutputReferences[2] = {BINDING::NORMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	gBufferOutputReferences[3] = {BINDING::MATERIAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkAttachmentReference depthOutputReference{};
	depthOutputReference.attachment = BINDING::DEPTH;
	depthOutputReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription gBufferSubpass{};
	gBufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	gBufferSubpass.colorAttachmentCount = static_cast<uint32_t>(gBufferOutputReferences.size());
	gBufferSubpass.pColorAttachments = gBufferOutputReferences.data();
	gBufferSubpass.pDepthStencilAttachment = &depthOutputReference;

	// SSAO subpass settings
	VkAttachmentReference ssaoOutputReference{};
	ssaoOutputReference.attachment = BINDING::SSAO;
	ssaoOutputReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 6> ssaoInputReference;
	ssaoInputReference[0] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
	ssaoInputReference[1] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
	ssaoInputReference[2] = {BINDING::NORMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	ssaoInputReference[3] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
	ssaoInputReference[4] = {BINDING::DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
	ssaoInputReference[5] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};


	VkSubpassDescription ssaoSubpass{};
	ssaoSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	ssaoSubpass.colorAttachmentCount = 1;
	ssaoSubpass.pColorAttachments = &ssaoOutputReference;
	ssaoSubpass.inputAttachmentCount = static_cast<uint32_t>(ssaoInputReference.size());
	ssaoSubpass.pInputAttachments = ssaoInputReference.data();

	// Lighting subpass settings
	VkAttachmentReference swapchainReference{};
	swapchainReference.attachment = 6;
	swapchainReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 6> lightingInputReferences;
	lightingInputReferences[0] = {BINDING::ALBEDO, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	lightingInputReferences[1] = {BINDING::POSITION, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	lightingInputReferences[2] = {BINDING::NORMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	lightingInputReferences[3] = {BINDING::MATERIAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	lightingInputReferences[4] = {BINDING::DEPTH, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
	lightingInputReferences[5] = {BINDING::SSAO, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

	VkSubpassDescription lightingSubpass{};
	lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	lightingSubpass.colorAttachmentCount = 1;
	lightingSubpass.pColorAttachments = &swapchainReference;
	lightingSubpass.inputAttachmentCount = static_cast<uint32_t>(lightingInputReferences.size());
	lightingSubpass.pInputAttachments = lightingInputReferences.data();

	VkSubpassDependency gBufferDependency{};
	gBufferDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	gBufferDependency.dstSubpass = 0;
	gBufferDependency.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	gBufferDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	gBufferDependency.srcAccessMask = VK_ACCESS_NONE;
	gBufferDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferDependency.dependencyFlags = 0;

	VkSubpassDependency ssaoDependency{};
	ssaoDependency.srcSubpass = 0;
	ssaoDependency.dstSubpass = 1;
	ssaoDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	ssaoDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	ssaoDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	ssaoDependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	ssaoDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDependency lightingDependency{};
	lightingDependency.srcSubpass = 1;
	lightingDependency.dstSubpass = 2;
	lightingDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	lightingDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	lightingDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	lightingDependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	lightingDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	std::array<VkAttachmentDescription, 7> attachments = {
		albedoAttachment,
		positionAttachment,
		normalAttachment,
		materialAttachment,
		depthAttachment,
		ssaoAttachment,
		swapchainAttachment
	};
	std::array<VkSubpassDescription, 3> subpasses = {gBufferSubpass, ssaoSubpass, lightingSubpass};
	std::array<VkSubpassDependency, 3> dependencies = {gBufferDependency, ssaoDependency, lightingDependency};
	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	createInfo.pSubpasses = subpasses.data();
	createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	createInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(device, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass");
	}
}

void DeferredRenderPass::createGBuffers() {
	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		albedo.image,
		albedo.imageMemory
	);
	albedo.imageView = VulkanUtils::createImageView(device, albedo.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		position.image,
		position.imageMemory
	);
	position.imageView = VulkanUtils::createImageView(device, position.image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		normal.image,
		normal.imageMemory
	);
	normal.imageView = VulkanUtils::createImageView(device, normal.image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		material.image,
		material.imageMemory
	);
	material.imageView = VulkanUtils::createImageView(device, material.image, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth.image,
		depth.imageMemory
	);
	depth.imageView = VulkanUtils::createImageView(device, depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		ssao.image,
		ssao.imageMemory
	);
	ssao.imageView = VulkanUtils::createImageView(device, ssao.image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void DeferredRenderPass::createFramebuffers() {
	framebuffers.resize(swapchain.imageViews.size());

	for (size_t i = 0; i < swapchain.imageViews.size(); ++i) {
		std::array<VkImageView, 7> attachments = {
			albedo.imageView,
			position.imageView,
			normal.imageView,
			material.imageView,
			depth.imageView,
			ssao.imageView,
			swapchain.imageViews[i],
		};

		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = swapchain.extent.width;
		createInfo.height = swapchain.extent.height;
		createInfo.layers = 1;

		if (vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer");
		}
	}
}

void DeferredRenderPass::createGraphicsPipeline() {
	auto vsGBufferCode = VulkanUtils::readFile("../shaders/deferred_gbuffer_vert.spv");
	auto fsGBufferCode = VulkanUtils::readFile("../shaders/deferred_gbuffer_frag.spv");
	auto vsSSAOCode = VulkanUtils::readFile("../shaders/screen_quad_vert.spv");
	auto fsSSAOCode = VulkanUtils::readFile("../shaders/ssao_frag.spv");
	auto vsLightingCode = VulkanUtils::readFile("../shaders/screen_quad_vert.spv");
	auto fsLightingCode = VulkanUtils::readFile("../shaders/deferred_lighting_frag.spv");

	VkShaderModule vsGBufferModule = VulkanUtils::createShaderModule(device, vsGBufferCode);
	VkShaderModule fsGBufferModule = VulkanUtils::createShaderModule(device, fsGBufferCode);
	VkShaderModule vsSSAOModule = VulkanUtils::createShaderModule(device, vsSSAOCode);
	VkShaderModule fsSSAOModule = VulkanUtils::createShaderModule(device, fsSSAOCode);
	VkShaderModule vsLightingModule = VulkanUtils::createShaderModule(device, vsLightingCode);
	VkShaderModule fsLightingModule = VulkanUtils::createShaderModule(device, fsLightingCode);

	VkPipelineShaderStageCreateInfo vsGBufferStageInfo{};
	vsGBufferStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vsGBufferStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vsGBufferStageInfo.module = vsGBufferModule;
	vsGBufferStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fsGBufferStageInfo{};
	fsGBufferStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fsGBufferStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fsGBufferStageInfo.module = fsGBufferModule;
	fsGBufferStageInfo.pName = "main";


	VkPipelineShaderStageCreateInfo vsSSAOStageInfo{};
	vsSSAOStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vsSSAOStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vsSSAOStageInfo.module = vsSSAOModule;
	vsSSAOStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fsSSAOStageInfo{};
	fsSSAOStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fsSSAOStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fsSSAOStageInfo.module = fsSSAOModule;
	fsSSAOStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo vsLightingStageInfo{};
	vsLightingStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vsLightingStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vsLightingStageInfo.module = vsLightingModule;
	vsLightingStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fsLightingStageInfo{};
	fsLightingStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fsLightingStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fsLightingStageInfo.module = fsLightingModule;
	fsLightingStageInfo.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> gBufferStages{vsGBufferStageInfo, fsGBufferStageInfo};
	std::array<VkPipelineShaderStageCreateInfo, 2> ssaoStages{vsSSAOStageInfo, fsSSAOStageInfo};
	std::array<VkPipelineShaderStageCreateInfo, 2> lightingStages{vsLightingStageInfo, fsLightingStageInfo};

	std::array<VkDescriptorSetLayout, 4> gBufferLayouts{
		commonDescriptor.modelMatrix.layout,
		commonDescriptor.cameraMatrix.layout,
		commonDescriptor.camera.layout,
		modelTextureDescriptorSetLayout
	};
	VkPipelineLayoutCreateInfo gBufferLayoutInfo{};
	gBufferLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	gBufferLayoutInfo.setLayoutCount = static_cast<uint32_t>(gBufferLayouts.size());
	gBufferLayoutInfo.pSetLayouts = gBufferLayouts.data();

	if (vkCreatePipelineLayout(device, &gBufferLayoutInfo, nullptr, &gBufferPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}

	VkPushConstantRange ssaoPushConstant{};
	ssaoPushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	ssaoPushConstant.offset = 0;
	ssaoPushConstant.size = sizeof(SSAOPushConstant);

	std::array<VkDescriptorSetLayout, 3> ssaoLayouts{
		commonDescriptor.cameraMatrix.layout,
		commonDescriptor.camera.layout,
		ssaoDescriptor.layout
	};
	VkPipelineLayoutCreateInfo ssaoLayoutInfo{};
	ssaoLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	ssaoLayoutInfo.setLayoutCount = static_cast<uint32_t>(ssaoLayouts.size());
	ssaoLayoutInfo.pSetLayouts = ssaoLayouts.data();
	ssaoLayoutInfo.pushConstantRangeCount = 1;
	ssaoLayoutInfo.pPushConstantRanges = &ssaoPushConstant;

	if (vkCreatePipelineLayout(device, &ssaoLayoutInfo, nullptr, &ssaoPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}

	std::array<VkDescriptorSetLayout, 6> lightingLayouts{
		commonDescriptor.cameraMatrix.layout,
		commonDescriptor.camera.layout,
		commonDescriptor.light.layout,
		lightingDescriptor.layout,
		shadowPass->getShadowMapLayout(),
		shadowPass->getLightMatrixLayout()
	};
	VkPipelineLayoutCreateInfo lightingLayoutInfo{};
	lightingLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	lightingLayoutInfo.setLayoutCount = static_cast<uint32_t>(lightingLayouts.size());
	lightingLayoutInfo.pSetLayouts = lightingLayouts.data();

	if (vkCreatePipelineLayout(device, &lightingLayoutInfo, nullptr, &lightingPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	std::array<VkPipelineColorBlendAttachmentState, 4> colorBlendAttachments{};
	for (int i = 0; i < colorBlendAttachments.size(); ++i) {
		colorBlendAttachments[i].blendEnable = VK_FALSE;
		colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.pAttachments = colorBlendAttachments.data();
	for (auto constant : colorBlending.blendConstants) {
		constant = 0.0f;
	}

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

    // change some settings for gbuffer-subpass
	VkPipelineColorBlendStateCreateInfo gBufferColorBlending = colorBlending;
	gBufferColorBlending.attachmentCount = 4;

	VkGraphicsPipelineCreateInfo gBufferPipelineInfo{};
	gBufferPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gBufferPipelineInfo.stageCount = static_cast<uint32_t>(gBufferStages.size());
	gBufferPipelineInfo.pStages = gBufferStages.data();
	gBufferPipelineInfo.pVertexInputState = &vertexInputInfo;
	gBufferPipelineInfo.pInputAssemblyState = &inputAssembly;
	gBufferPipelineInfo.pViewportState = &viewportState;
	gBufferPipelineInfo.pRasterizationState = &rasterizer;
	gBufferPipelineInfo.pMultisampleState = &multisampling;
	gBufferPipelineInfo.pDepthStencilState = &depthStencil;
	gBufferPipelineInfo.pColorBlendState = &gBufferColorBlending;
	gBufferPipelineInfo.pDynamicState = &dynamicState;
	gBufferPipelineInfo.layout = gBufferPipelineLayout;
	gBufferPipelineInfo.renderPass = renderPass;
	gBufferPipelineInfo.subpass = 0;
	gBufferPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gBufferPipelineInfo, nullptr, &gBufferPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	// change some settings for ssao-subpass
	VkPipelineVertexInputStateCreateInfo squadVertexInputInfo = vertexInputInfo;
	squadVertexInputInfo.vertexBindingDescriptionCount = 0;
	squadVertexInputInfo.pVertexBindingDescriptions = nullptr;
	squadVertexInputInfo.vertexAttributeDescriptionCount = 0;
	squadVertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo squadInputAssembly = inputAssembly;
	squadInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	VkGraphicsPipelineCreateInfo ssaoPipelineInfo{};
	ssaoPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	ssaoPipelineInfo.stageCount = static_cast<uint32_t>(ssaoStages.size());
	ssaoPipelineInfo.pStages = ssaoStages.data();
	ssaoPipelineInfo.pVertexInputState = &squadVertexInputInfo;
	ssaoPipelineInfo.pInputAssemblyState = &squadInputAssembly;
	ssaoPipelineInfo.pViewportState = &viewportState;
	ssaoPipelineInfo.pRasterizationState = &rasterizer;
	ssaoPipelineInfo.pMultisampleState = &multisampling;
	ssaoPipelineInfo.pDepthStencilState = &depthStencil;
	ssaoPipelineInfo.pColorBlendState = &colorBlending;
	ssaoPipelineInfo.pDynamicState = &dynamicState;
	ssaoPipelineInfo.layout = ssaoPipelineLayout;
	ssaoPipelineInfo.renderPass = renderPass;
	ssaoPipelineInfo.subpass = 1;
	ssaoPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ssaoPipelineInfo, nullptr, &ssaoPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	VkGraphicsPipelineCreateInfo lightingPipelineInfo{};
	lightingPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	lightingPipelineInfo.stageCount = static_cast<uint32_t>(lightingStages.size());
	lightingPipelineInfo.pStages = lightingStages.data();
	lightingPipelineInfo.pVertexInputState = &squadVertexInputInfo;
	lightingPipelineInfo.pInputAssemblyState = &squadInputAssembly;
	lightingPipelineInfo.pViewportState = &viewportState;
	lightingPipelineInfo.pRasterizationState = &rasterizer;
	lightingPipelineInfo.pMultisampleState = &multisampling;
	lightingPipelineInfo.pDepthStencilState = &depthStencil;
	lightingPipelineInfo.pColorBlendState = &colorBlending;
	lightingPipelineInfo.pDynamicState = &dynamicState;
	lightingPipelineInfo.layout = lightingPipelineLayout;
	lightingPipelineInfo.renderPass = renderPass;
	lightingPipelineInfo.subpass = 2;
	lightingPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &lightingPipelineInfo, nullptr, &lightingPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	vkDestroyShaderModule(device, fsGBufferModule, nullptr);
	vkDestroyShaderModule(device, fsSSAOModule, nullptr);
	vkDestroyShaderModule(device, fsLightingModule, nullptr);
	vkDestroyShaderModule(device, vsGBufferModule, nullptr);
	vkDestroyShaderModule(device, vsSSAOModule, nullptr);
	vkDestroyShaderModule(device, vsLightingModule, nullptr);
}

void DeferredRenderPass::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding albedoBinding{};
	albedoBinding.binding = BINDING::ALBEDO;
	albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	albedoBinding.descriptorCount = 1;
	albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	albedoBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding positionBinding{};
	positionBinding.binding = BINDING::POSITION;
	positionBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	positionBinding.descriptorCount = 1;
	positionBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	positionBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding normalBinding{};
	normalBinding.binding = BINDING::NORMAL;
	normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	normalBinding.descriptorCount = 1;
	normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	normalBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialBinding{};
	materialBinding.binding = BINDING::MATERIAL;
	materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	materialBinding.descriptorCount = 1;
	materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding depthBinding{};
	depthBinding.binding = BINDING::DEPTH;
	depthBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthBinding.descriptorCount = 1;
	depthBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	depthBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding ssaoBinding{};
	ssaoBinding.binding = BINDING::SSAO;
	ssaoBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	ssaoBinding.descriptorCount = 1;
	ssaoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	ssaoBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> ssaoInputLayoutBindings{
		normalBinding,
		depthBinding
	};

	VkDescriptorSetLayoutCreateInfo ssaoInputLayoutInfo{};
	ssaoInputLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ssaoInputLayoutInfo.bindingCount = static_cast<uint32_t>(ssaoInputLayoutBindings.size());
	ssaoInputLayoutInfo.pBindings = ssaoInputLayoutBindings.data();

	if(vkCreateDescriptorSetLayout(device, &ssaoInputLayoutInfo, nullptr, &ssaoDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ssao descriptor set layout");
	}

	std::array<VkDescriptorSetLayoutBinding, 6> lightingInputLayoutBindings{
		albedoBinding,
		normalBinding,
		positionBinding,
		materialBinding,
		depthBinding,
		ssaoBinding
	};

	VkDescriptorSetLayoutCreateInfo lightingInputLayoutInfo{};
	lightingInputLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lightingInputLayoutInfo.bindingCount = static_cast<uint32_t>(lightingInputLayoutBindings.size());
	lightingInputLayoutInfo.pBindings = lightingInputLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(device, &lightingInputLayoutInfo, nullptr, &lightingDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create lighting descriptor set layout");
	}
}

void DeferredRenderPass::createDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(Config::MAX_FRAMES_IN_FLIGHT * 7);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(Config::MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();
	createInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT * poolSizes.size();

	if (vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool");
	}
}

void DeferredRenderPass::createDescriptorSets() {
	std::vector<VkDescriptorSetLayout> ssaoInputLayouts(Config::MAX_FRAMES_IN_FLIGHT, ssaoDescriptor.layout);

	VkDescriptorSetAllocateInfo ssaoInputAllocInfo{};
	ssaoInputAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ssaoInputAllocInfo.descriptorPool = descriptorPool;
	ssaoInputAllocInfo.descriptorSetCount = static_cast<uint32_t>(ssaoInputLayouts.size());
	ssaoInputAllocInfo.pSetLayouts = ssaoInputLayouts.data();

	ssaoDescriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &ssaoInputAllocInfo, ssaoDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	std::vector<VkDescriptorSetLayout> lightingInputLayouts(Config::MAX_FRAMES_IN_FLIGHT, lightingDescriptor.layout);

	VkDescriptorSetAllocateInfo lightingInputAllocInfo{};
	lightingInputAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	lightingInputAllocInfo.descriptorPool = descriptorPool;
	lightingInputAllocInfo.descriptorSetCount = static_cast<uint32_t>(lightingInputLayouts.size());
	lightingInputAllocInfo.pSetLayouts = lightingInputLayouts.data();

	lightingDescriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &lightingInputAllocInfo, lightingDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorImageInfo albedoImageInfo{};
		albedoImageInfo.imageView = albedo.imageView;
		albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo positionImageInfo{};
		positionImageInfo.imageView = position.imageView;
		positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normalImageInfo{};
		normalImageInfo.imageView = normal.imageView;
		normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo materialImageInfo{};
		materialImageInfo.imageView = material.imageView;
		materialImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo depthImageInfo{};
		depthImageInfo.imageView = depth.imageView;
		depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo ssaoImageInfo{};
		ssaoImageInfo.imageView = ssao.imageView;
		ssaoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<VkWriteDescriptorSet, 2> ssaoDescriptorWrites{};
		ssaoDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ssaoDescriptorWrites[0].dstSet = ssaoDescriptor.sets[i];
		ssaoDescriptorWrites[0].dstBinding = BINDING::NORMAL;
		ssaoDescriptorWrites[0].dstArrayElement = 0;
		ssaoDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		ssaoDescriptorWrites[0].descriptorCount = 1;
		ssaoDescriptorWrites[0].pImageInfo = &normalImageInfo;

		ssaoDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ssaoDescriptorWrites[1].dstSet = ssaoDescriptor.sets[i];
		ssaoDescriptorWrites[1].dstBinding = BINDING::DEPTH;
		ssaoDescriptorWrites[1].dstArrayElement = 0;
		ssaoDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		ssaoDescriptorWrites[1].descriptorCount = 1;
		ssaoDescriptorWrites[1].pImageInfo = &depthImageInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(ssaoDescriptorWrites.size()), ssaoDescriptorWrites.data(), 0, nullptr);

		std::array<VkWriteDescriptorSet, 6> lightingDescriptorWrites{};
		lightingDescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightingDescriptorWrites[0].dstSet = lightingDescriptor.sets[i];
		lightingDescriptorWrites[0].dstBinding = BINDING::ALBEDO;
		lightingDescriptorWrites[0].dstArrayElement = 0;
		lightingDescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		lightingDescriptorWrites[0].descriptorCount = 1;
		lightingDescriptorWrites[0].pImageInfo = &albedoImageInfo;

		lightingDescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightingDescriptorWrites[1].dstSet = lightingDescriptor.sets[i];
		lightingDescriptorWrites[1].dstBinding = BINDING::POSITION;
		lightingDescriptorWrites[1].dstArrayElement = 0;
		lightingDescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		lightingDescriptorWrites[1].descriptorCount = 1;
		lightingDescriptorWrites[1].pImageInfo = &positionImageInfo;

		lightingDescriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightingDescriptorWrites[2].dstSet = lightingDescriptor.sets[i];
		lightingDescriptorWrites[2].dstBinding = BINDING::NORMAL;
		lightingDescriptorWrites[2].dstArrayElement = 0;
		lightingDescriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		lightingDescriptorWrites[2].descriptorCount = 1;
		lightingDescriptorWrites[2].pImageInfo = &normalImageInfo;

		lightingDescriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightingDescriptorWrites[3].dstSet = lightingDescriptor.sets[i];
		lightingDescriptorWrites[3].dstBinding = BINDING::MATERIAL;
		lightingDescriptorWrites[3].dstArrayElement = 0;
		lightingDescriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		lightingDescriptorWrites[3].descriptorCount = 1;
		lightingDescriptorWrites[3].pImageInfo = &materialImageInfo;

		lightingDescriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightingDescriptorWrites[4].dstSet = lightingDescriptor.sets[i];
		lightingDescriptorWrites[4].dstBinding = BINDING::DEPTH;
		lightingDescriptorWrites[4].dstArrayElement = 0;
		lightingDescriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		lightingDescriptorWrites[4].descriptorCount = 1;
		lightingDescriptorWrites[4].pImageInfo = &depthImageInfo;

		lightingDescriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightingDescriptorWrites[5].dstSet = lightingDescriptor.sets[i];
		lightingDescriptorWrites[5].dstBinding = BINDING::SSAO;
		lightingDescriptorWrites[5].dstArrayElement = 0;
		lightingDescriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		lightingDescriptorWrites[5].descriptorCount = 1;
		lightingDescriptorWrites[5].pImageInfo = &ssaoImageInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(lightingDescriptorWrites.size()), lightingDescriptorWrites.data(), 0, nullptr);
	}
}

void DeferredRenderPass::render(
	std::vector<VkCommandBuffer>& commandBuffers,
	uint32_t imageIndex,
	uint32_t currentFrame,
	std::vector<void*>& modelMatrixBuffersMapped,
	const std::vector<AssetData>& models,
	const Camera& camera,
	const std::vector<DirectionalLightBuffer>& directionalLights,
	GLFWwindow* window
) {
	shadowPass->generateShadowMap(
		commandBuffers,
		imageIndex,
		currentFrame,
		modelMatrixBuffersMapped,
		models,
		camera,
		directionalLights,
		window
	);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapchain.extent;

	std::array<VkClearValue, 7> clearValues{};
	clearValues[0].color = {{0.5f, 0.8f, 1.0f, 0.7f}};
	clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clearValues[3].color = {{0.0f, 0.0f}};
	clearValues[4].depthStencil = {1.0f, 0};
	clearValues[5].color = {0.0f};
	clearValues[6].color = {{0.0f, 0.0f, 0.0f, 0.0f}};

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapchain.extent.width);
		viewport.height = static_cast<float>(swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swapchain.extent;
		vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferPipeline);

		// G-Buffer subpass
		for(size_t i = 0; i < models.size(); ++i) {
			uint32_t offset = static_cast<uint32_t>(i * sizeof(TransformMatrixBuffer));
			TransformMatrixBuffer matrixUBO{};
			matrixUBO.model = models[i].object.getModelMatrix();
			void* target = static_cast<char*>(modelMatrixBuffersMapped[currentFrame]) + offset;
			memcpy(target, &matrixUBO, sizeof(matrixUBO));
			VkBuffer vertexBuffers[] = {models[i].resource.vertexBufferResource.buffer};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffers[currentFrame], models[i].resource.indexBufferResource.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				gBufferPipelineLayout,
				0,
				1,
				&commonDescriptor.modelMatrix.sets[currentFrame],
				1,
				&offset
			);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				gBufferPipelineLayout,
				1,
				1,
				&commonDescriptor.cameraMatrix.sets[currentFrame],
				0,
				nullptr
			);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				gBufferPipelineLayout,
				2,
				1,
				&commonDescriptor.camera.sets[currentFrame],
				0,
				nullptr
			);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				gBufferPipelineLayout,
				3,
				1,
				&models[i].resource.descriptorSets[currentFrame],
				0,
				nullptr
			);
			vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(models[i].resource.indexCount), 1, 0, 0, 0);
		}

		vkCmdNextSubpass(commandBuffers[currentFrame], VK_SUBPASS_CONTENTS_INLINE);

		// SSAO subpass
		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ssaoPipelineLayout,
			0,
			1,
			&commonDescriptor.cameraMatrix.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ssaoPipelineLayout,
			1,
			1,
			&commonDescriptor.camera.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			ssaoPipelineLayout,
			2,
			1,
			&ssaoDescriptor.sets[currentFrame],
			0,
			nullptr
		);
		SSAOPushConstant pushConstant;
		pushConstant.screenSize = glm::vec2(swapchain.extent.width, swapchain.extent.height);
		vkCmdPushConstants(
			commandBuffers[currentFrame],
			ssaoPipelineLayout,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(SSAOPushConstant),
			&pushConstant
		);

		vkCmdDraw(commandBuffers[currentFrame], 4, 1, 0, 0);

		vkCmdNextSubpass(commandBuffers[currentFrame], VK_SUBPASS_CONTENTS_INLINE);

		// Lighting subpass
		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelineLayout,
			0,
			1,
			&commonDescriptor.cameraMatrix.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelineLayout,
			1,
			1,
			&commonDescriptor.camera.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelineLayout,
			2,
			1,
			&commonDescriptor.light.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelineLayout,
			3,
			1,
			&lightingDescriptor.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelineLayout,
			4,
			1,
			&shadowPass->getShadowMap()[currentFrame],
			0,
			nullptr
		);
		vkCmdBindDescriptorSets(
			commandBuffers[currentFrame],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelineLayout,
			5,
			1,
			&shadowPass->getLightMatrix()[currentFrame],
			0,
			nullptr
		);

		vkCmdDraw(commandBuffers[currentFrame], 4, 1, 0, 0);
	} vkCmdEndRenderPass(commandBuffers[currentFrame]);
}