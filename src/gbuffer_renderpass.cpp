#include "gbuffer_renderpass.hpp"

#include <vulkan/vulkan.h>

#include <iostream>
#include <cstddef>
#include <stdexcept>
#include <array>
#include <vector>
#include <memory>

#include "vulkan_types.hpp"
#include "vulkan_utils.hpp"
#include "constants.hpp"
#include "vulkan_vertex.hpp"

enum BINDING {
	ALBEDO = 0,
	POSITION = 1,
	NORMAL = 2,
	MATERIAL = 3,
	DEPTH = 4
};

void GBufferRenderPass::init() {
	createRenderPass();
	createSampler();
	createImageResources();
	createGraphicsPipeline();
}

void GBufferRenderPass::createImageResources(){
	createGBufferResources();
	createFramebuffers();
	createDescriptorSetLayout();
	createDescriptorPool();
	createDescriptorSets();
}

void GBufferRenderPass::cleanup() {
	cleanupImageResources();
	vkDestroySampler(device, sampler, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
}

void GBufferRenderPass::cleanupImageResources() {
	albedo.cleanup(device);
	position.cleanup(device);
	normal.cleanup(device);
	material.cleanup(device);
	depth.cleanup(device);

	for (auto framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	descriptor.cleanup(device);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}

void GBufferRenderPass::generateGBuffer(
	std::vector<VkCommandBuffer>& commandBuffers,
	uint32_t imageIndex,
	uint32_t currentFrame,
	std::vector<void*>& modelMatrixBuffersMapped,
	const std::vector<AssetData>& models
) {
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapchain.extent;

	std::array<VkClearValue, 7> clearValues{};
	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clearValues[3].color = {{0.0f, 0.0f}};
	clearValues[4].depthStencil = {1.0f, 0};

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

		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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
				pipelineLayout,
				0,
				1,
				&commonDescriptor.modelMatrix.sets[currentFrame],
				1,
				&offset
			);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				1,
				1,
				&commonDescriptor.cameraMatrix.sets[currentFrame],
				0,
				nullptr
			);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				2,
				1,
				&commonDescriptor.camera.sets[currentFrame],
				0,
				nullptr
			);
			vkCmdBindDescriptorSets(
				commandBuffers[currentFrame],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				3,
				1,
				&models[i].resource.descriptorSets[currentFrame],
				0,
				nullptr
			);
			vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(models[i].resource.indexCount), 1, 0, 0, 0);
		}

	} vkCmdEndRenderPass(commandBuffers[currentFrame]);

	transitionGBufferToSampler(commandBuffers[currentFrame]);
}

void GBufferRenderPass::createRenderPass() {
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

	VkSubpassDependency gBufferDependency{};
	gBufferDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	gBufferDependency.dstSubpass = 0;
	gBufferDependency.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	gBufferDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	gBufferDependency.srcAccessMask = VK_ACCESS_NONE;
	gBufferDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferDependency.dependencyFlags = 0;

	std::array<VkAttachmentDescription, 5> attachments = {
		albedoAttachment,
		positionAttachment,
		normalAttachment,
		materialAttachment,
		depthAttachment
	};
	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &gBufferSubpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &gBufferDependency;

	if (vkCreateRenderPass(device, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass");
	}
}

void GBufferRenderPass::createGBufferResources() {
	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth.image,
		depth.imageMemory
	);
	depth.imageView = VulkanUtils::createImageView(device, depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void GBufferRenderPass::createFramebuffers() {
	framebuffers.resize(swapchain.imageViews.size());

	for (size_t i = 0; i < swapchain.imageViews.size(); ++i) {
		std::array<VkImageView, 5> attachments = {
			albedo.imageView,
			position.imageView,
			normal.imageView,
			material.imageView,
			depth.imageView
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

void GBufferRenderPass::createGraphicsPipeline() {
	auto vsGBufferCode = VulkanUtils::readFile("../shaders/deferred_gbuffer_vert.spv");
	auto fsGBufferCode = VulkanUtils::readFile("../shaders/deferred_gbuffer_frag.spv");

	VkShaderModule vsGBufferModule = VulkanUtils::createShaderModule(device, vsGBufferCode);
	VkShaderModule fsGBufferModule = VulkanUtils::createShaderModule(device, fsGBufferCode);

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

	std::array<VkPipelineShaderStageCreateInfo, 2> gBufferStages{vsGBufferStageInfo, fsGBufferStageInfo};

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

	if (vkCreatePipelineLayout(device, &gBufferLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
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
	gBufferPipelineInfo.layout = pipelineLayout;
	gBufferPipelineInfo.renderPass = renderPass;
	gBufferPipelineInfo.subpass = 0;
	gBufferPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gBufferPipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	vkDestroyShaderModule(device, fsGBufferModule, nullptr);
	vkDestroyShaderModule(device, vsGBufferModule, nullptr);
}

void GBufferRenderPass::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding albedoBinding{};
	albedoBinding.binding = BINDING::ALBEDO;
	albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	albedoBinding.descriptorCount = 1;
	albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	albedoBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding positionBinding{};
	positionBinding.binding = BINDING::POSITION;
	positionBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	positionBinding.descriptorCount = 1;
	positionBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	positionBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding normalBinding{};
	normalBinding.binding = BINDING::NORMAL;
	normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalBinding.descriptorCount = 1;
	normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	normalBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialBinding{};
	materialBinding.binding = BINDING::MATERIAL;
	materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	materialBinding.descriptorCount = 1;
	materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding depthBinding{};
	depthBinding.binding = BINDING::DEPTH;
	depthBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	depthBinding.descriptorCount = 1;
	depthBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	depthBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 5> layoutBindings{
		albedoBinding,
		normalBinding,
		positionBinding,
		materialBinding,
		depthBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutInfo.pBindings = layoutBindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create gbuffer descriptor set layout");
	}
}

void GBufferRenderPass::createDescriptorPool() {
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

void GBufferRenderPass::createDescriptorSets() {

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(Config::MAX_FRAMES_IN_FLIGHT, descriptor.layout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	allocInfo.pSetLayouts = descriptorSetLayouts.data();

	descriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	VkDescriptorImageInfo albedoImageInfo{};
	albedoImageInfo.imageView = albedo.imageView;
	albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoImageInfo.sampler = sampler;

	VkDescriptorImageInfo positionImageInfo{};
	positionImageInfo.imageView = position.imageView;
	positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	positionImageInfo.sampler = sampler;

	VkDescriptorImageInfo normalImageInfo{};
	normalImageInfo.imageView = normal.imageView;
	normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalImageInfo.sampler = sampler;

	VkDescriptorImageInfo materialImageInfo{};
	materialImageInfo.imageView = material.imageView;
	materialImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	materialImageInfo.sampler = sampler;

	VkDescriptorImageInfo depthImageInfo{};
	depthImageInfo.imageView = depth.imageView;
	depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depthImageInfo.sampler = sampler;

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptor.sets[i];
		descriptorWrites[0].dstBinding = BINDING::ALBEDO;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &albedoImageInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptor.sets[i];
		descriptorWrites[1].dstBinding = BINDING::POSITION;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &positionImageInfo;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = descriptor.sets[i];
		descriptorWrites[2].dstBinding = BINDING::NORMAL;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &normalImageInfo;

		descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[3].dstSet = descriptor.sets[i];
		descriptorWrites[3].dstBinding = BINDING::MATERIAL;
		descriptorWrites[3].dstArrayElement = 0;
		descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].pImageInfo = &materialImageInfo;

		descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[4].dstSet = descriptor.sets[i];
		descriptorWrites[4].dstBinding = BINDING::DEPTH;
		descriptorWrites[4].dstArrayElement = 0;
		descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[4].descriptorCount = 1;
		descriptorWrites[4].pImageInfo = &depthImageInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void GBufferRenderPass::createSampler() {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 1.0f;
	createInfo.mipLodBias = 0.0f;

	if (vkCreateSampler(device, &createInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler");
	}
}

void GBufferRenderPass::transitionGBufferToSampler(VkCommandBuffer commandBuffer) {
	VulkanUtils::transitionLayout(
		commandBuffer,
		albedo.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		position.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		normal.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		material.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		depth.image,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);

	isTransitioned = true;
}

void GBufferRenderPass::transitionGBufferToAttachment(VkCommandBuffer commandBuffer) {
	VulkanUtils::transitionLayout(
		commandBuffer,
		albedo.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		position.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		normal.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		material.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	);
	VulkanUtils::transitionLayout(
		commandBuffer,
		depth.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
	);

	isTransitioned = false;
}