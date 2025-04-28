#include "swapchain_renderpass.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <stdexcept>
#include <memory>
#include <array>
#include <iostream>

#include "vulkan_types.hpp"
#include "constants.hpp"
#include "vulkan_utils.hpp"

void SwapchainRenderPass::init() {
	createSampler();
	createRenderedImage();
	createDescriptorPool();
	createDescriptor();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
}

void SwapchainRenderPass::cleanup() {
	cleanupImageResources();
	vkDestroySampler(device, sampler, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
}

void SwapchainRenderPass::render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame) {
	VkFramebuffer framebuffer = framebuffers[imageIndex];
	VkClearValue clearColor = { {0.0f, 0.0f, 0.0f, 1.0f} };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = framebuffer;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = swapchain.extent;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearColor;

	VulkanUtils::transitionLayout(
		commandBuffer,
		renderedImageResource.image,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	{

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapchain.extent.width);
		viewport.height = static_cast<float>(swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swapchain.extent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&samplerDescriptor.sets[currentFrame],
			0,
			nullptr
		);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	} vkCmdEndRenderPass(commandBuffer);

	VulkanUtils::transitionLayout(
		commandBuffer,
		renderedImageResource.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	);
}

void SwapchainRenderPass::createImageResources(){
	createRenderedImage();
	createFramebuffers();
	createDescriptorPool();
	createDescriptor();
}

void SwapchainRenderPass::cleanupImageResources() {
	renderedImageResource.cleanup(device);
	renderedImageDescriptor.cleanup(device);
	samplerDescriptor.cleanup(device);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	for (auto framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
}

void SwapchainRenderPass::createRenderedImage() {
	VulkanUtils::createImage(
		physicalDevice,
		device,
		swapchain.extent.width,
		swapchain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		renderedImageResource.image,
		renderedImageResource.imageMemory
	);
	renderedImageResource.imageView = VulkanUtils::createImageView(device, renderedImageResource.image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);
	{
		VulkanUtils::transitionLayout(
			commandBuffer,
			renderedImageResource.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	} VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
}

void SwapchainRenderPass::createDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[0].descriptorCount = Config::MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = Config::MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT + Config::MAX_FRAMES_IN_FLIGHT;

	if(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain descriptor pool");
	}
}

void SwapchainRenderPass::createDescriptor() {
	VkDescriptorSetLayoutBinding renderedImageBinding{};
	renderedImageBinding.binding = 0;
	renderedImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	renderedImageBinding.descriptorCount = 1;
	renderedImageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	renderedImageBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo renderedImageLayoutInfo{};
	renderedImageLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	renderedImageLayoutInfo.bindingCount = 1;
	renderedImageLayoutInfo.pBindings = &renderedImageBinding;

	if (vkCreateDescriptorSetLayout(device, &renderedImageLayoutInfo, nullptr, &renderedImageDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain descriptor set layout");
	}

	std::vector<VkDescriptorSetLayout> renderedImageLayouts(Config::MAX_FRAMES_IN_FLIGHT, renderedImageDescriptor.layout);
	VkDescriptorSetAllocateInfo renderedImageAllocInfo{};
	renderedImageAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	renderedImageAllocInfo.descriptorPool = descriptorPool;
	renderedImageAllocInfo.descriptorSetCount = static_cast<uint32_t>(renderedImageLayouts.size());;
	renderedImageAllocInfo.pSetLayouts = renderedImageLayouts.data();

	renderedImageDescriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if(vkAllocateDescriptorSets(device, &renderedImageAllocInfo, renderedImageDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate swapchain descriptor set");
	}

	VkDescriptorImageInfo renderedImageImageInfo{};
	renderedImageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderedImageImageInfo.imageView = renderedImageResource.imageView;
	renderedImageImageInfo.sampler = sampler;

	for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
		VkWriteDescriptorSet renderedImageWrite{};
		renderedImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		renderedImageWrite.dstSet = renderedImageDescriptor.sets[i];
		renderedImageWrite.dstBinding = 0;
		renderedImageWrite.dstArrayElement = 0;
		renderedImageWrite.descriptorCount = 1;
		renderedImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		renderedImageWrite.pImageInfo = &renderedImageImageInfo;

		vkUpdateDescriptorSets(device, 1, &renderedImageWrite, 0, nullptr);
	}

	VkDescriptorSetLayoutBinding samplerBinding{};
	samplerBinding.binding = 0;
	samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerBinding.descriptorCount = 1;
	samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo samplerLayoutInfo{};
	samplerLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	samplerLayoutInfo.bindingCount = 1;
	samplerLayoutInfo.pBindings = &samplerBinding;

	if (vkCreateDescriptorSetLayout(device, &samplerLayoutInfo, nullptr, &samplerDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain descriptor set layout");
	}

	std::vector<VkDescriptorSetLayout> samplerLayouts(Config::MAX_FRAMES_IN_FLIGHT, samplerDescriptor.layout);
	VkDescriptorSetAllocateInfo samplerAllocInfo{};
	samplerAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	samplerAllocInfo.descriptorPool = descriptorPool;
	samplerAllocInfo.descriptorSetCount = static_cast<uint32_t>(samplerLayouts.size());;
	samplerAllocInfo.pSetLayouts = samplerLayouts.data();

	samplerDescriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if(vkAllocateDescriptorSets(device, &samplerAllocInfo, samplerDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate swapchain descriptor set");
	}

	VkDescriptorImageInfo samplerImageInfo{};
	samplerImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	samplerImageInfo.imageView = renderedImageResource.imageView;
	samplerImageInfo.sampler = sampler;

	for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
		VkWriteDescriptorSet samplerWrite{};
		samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerWrite.dstSet = samplerDescriptor.sets[i];
		samplerWrite.dstBinding = 0;
		samplerWrite.dstArrayElement = 0;
		samplerWrite.descriptorCount = 1;
		samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerWrite.pImageInfo = &samplerImageInfo;

		vkUpdateDescriptorSets(device, 1, &samplerWrite, 0, nullptr);
	}
}

void SwapchainRenderPass::createSampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create renderedImage sampler");
	}
}

void SwapchainRenderPass::createRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain.imageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain render pass");
	}
}

void SwapchainRenderPass::createGraphicsPipeline() {
	auto vsCode = VulkanUtils::readFile("../shaders/screen_quad_vert.spv");
	auto fsCode = VulkanUtils::readFile("../shaders/swapchain_frag.spv");

	VkShaderModule vsModule = VulkanUtils::createShaderModule(device, vsCode);
	VkShaderModule fsModule = VulkanUtils::createShaderModule(device, fsCode);

	VkPipelineShaderStageCreateInfo vsStageInfo{};
	vsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vsStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vsStageInfo.module = vsModule;
	vsStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fsStageInfo{};
	fsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fsStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fsStageInfo.module = fsModule;
	fsStageInfo.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vsStageInfo, fsStageInfo};

	std::array<VkDescriptorSetLayout, 1> desctiptorSetlayouts{
		samplerDescriptor.layout
	};
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(desctiptorSetlayouts.size());
	pipelineLayoutInfo.pSetLayouts = desctiptorSetlayouts.data();

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}

	VkPipelineVertexInputStateCreateInfo squadVertexInputInfo{};
	squadVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	squadVertexInputInfo.vertexBindingDescriptionCount = 0;
	squadVertexInputInfo.pVertexBindingDescriptions = nullptr;
	squadVertexInputInfo.vertexAttributeDescriptionCount = 0;
	squadVertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
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

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &squadVertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	vkDestroyShaderModule(device, fsModule, nullptr);
	vkDestroyShaderModule(device, vsModule, nullptr);
}

void SwapchainRenderPass::createFramebuffers() {
	framebuffers.resize(swapchain.imageViews.size());
	for (size_t i = 0; i < swapchain.imageViews.size(); i++) {
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &swapchain.imageViews[i];
		framebufferInfo.width = swapchain.extent.width;
		framebufferInfo.height = swapchain.extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swapchain framebuffer");
		}
	}
}