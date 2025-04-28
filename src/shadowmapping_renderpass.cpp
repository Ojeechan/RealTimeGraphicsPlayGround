#include "shadowmapping_renderpass.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <stdexcept>

#include "vulkan_utils.hpp"
#include "constants.hpp"
#include "vulkan_vertex.hpp"
#include "camera.hpp"

struct ShadowMapLight {
	glm::mat4 view;
	glm::mat4 proj;
};

void BaseShadowRenderPass::init() {
	// create UBO (light view ...)
	shadowMapLight.buffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
	shadowMapLight.buffersMemory.resize(Config::MAX_FRAMES_IN_FLIGHT);
	shadowMapLight.buffersMapped.resize(Config::MAX_FRAMES_IN_FLIGHT);
	int bufferSize = sizeof(ShadowMapLight);

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VulkanUtils::createBuffer(
			physicalDevice,
			device,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			shadowMapLight.buffers[i],
			shadowMapLight.buffersMemory[i],
			nullptr
		);
		vkMapMemory(device, shadowMapLight.buffersMemory[i], 0, bufferSize, 0, &shadowMapLight.buffersMapped[i]);
	}

	// create image
	VulkanUtils::createImage(
		physicalDevice,
		device,
		SHADOW_MAP_RESOLUTION,
		SHADOW_MAP_RESOLUTION,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		shadowMap.image,
		shadowMap.imageMemory
	);
	shadowMap.imageView = VulkanUtils::createImageView(device, shadowMap.image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	// create descriptor set layout
	VkDescriptorSetLayoutBinding lightBinding{};
	lightBinding.binding = 0;
	lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightBinding.descriptorCount = 1;
	lightBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	lightBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo lightLayoutInfo{};
	lightLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lightLayoutInfo.bindingCount = 1;
	lightLayoutInfo.pBindings = &lightBinding;

	if (vkCreateDescriptorSetLayout(device, &lightLayoutInfo, nullptr, &lightDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout");
	}

	VkDescriptorSetLayoutBinding shadowMapBinding{};
	shadowMapBinding.binding = 0;
	shadowMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapBinding.descriptorCount = 1;
	shadowMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowMapBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo shadowMapLayoutInfo{};
	shadowMapLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	shadowMapLayoutInfo.bindingCount = 1;
	shadowMapLayoutInfo.pBindings = &shadowMapBinding;

	if (vkCreateDescriptorSetLayout(device, &shadowMapLayoutInfo, nullptr, &shadowMapDescriptor.layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout");
	}

	// create descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  Config::MAX_FRAMES_IN_FLIGHT},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Config::MAX_FRAMES_IN_FLIGHT}
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1 + Config::MAX_FRAMES_IN_FLIGHT * 10;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool");
	}

	// create sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_TRUE;
	samplerInfo.compareOp = VK_COMPARE_OP_LESS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map sampler");
	}

	// create descriptor set
	std::vector<VkDescriptorSetLayout> lightLayouts(Config::MAX_FRAMES_IN_FLIGHT, lightDescriptor.layout);
	VkDescriptorSetAllocateInfo lightAllocInfo{};
	lightAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	lightAllocInfo.descriptorPool = descriptorPool;
	lightAllocInfo.descriptorSetCount = static_cast<uint32_t>(lightLayouts.size());
	lightAllocInfo.pSetLayouts = lightLayouts.data();

	lightDescriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &lightAllocInfo, lightDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate shadow mapping lightUBO descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo lightBufferInfo{};
		lightBufferInfo.buffer = shadowMapLight.buffers[i];
		lightBufferInfo.offset = 0;
		lightBufferInfo.range = sizeof(ShadowMapLight);

		VkWriteDescriptorSet lightDescriptorWrite{};
		lightDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightDescriptorWrite.dstSet = lightDescriptor.sets[i];
		lightDescriptorWrite.dstBinding = 0;
		lightDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDescriptorWrite.descriptorCount = 1;
		lightDescriptorWrite.pBufferInfo = &lightBufferInfo;

		vkUpdateDescriptorSets(device, 1, &lightDescriptorWrite, 0, nullptr);
	}

	std::vector<VkDescriptorSetLayout> shadowMapLayouts(Config::MAX_FRAMES_IN_FLIGHT, shadowMapDescriptor.layout);
	VkDescriptorSetAllocateInfo shadowMapAllocInfo{};
	shadowMapAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	shadowMapAllocInfo.descriptorPool = descriptorPool;
	shadowMapAllocInfo.descriptorSetCount = static_cast<uint32_t>(shadowMapLayouts.size());
	shadowMapAllocInfo.pSetLayouts = shadowMapLayouts.data();

	shadowMapDescriptor.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &shadowMapAllocInfo, shadowMapDescriptor.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate shadow mapping lightUBO descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorImageInfo shadowMapImageInfo{};
		shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shadowMapImageInfo.imageView = shadowMap.imageView;
		shadowMapImageInfo.sampler = sampler;

		VkWriteDescriptorSet shadowMapDescriptorWrite{};
		shadowMapDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowMapDescriptorWrite.dstSet = shadowMapDescriptor.sets[i];
		shadowMapDescriptorWrite.dstBinding = 0;
		shadowMapDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowMapDescriptorWrite.descriptorCount = 1;
		shadowMapDescriptorWrite.pImageInfo = &shadowMapImageInfo;

		vkUpdateDescriptorSets(device, 1, &shadowMapDescriptorWrite, 0, nullptr);
	}

	// create render pass
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference{};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthReference;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &depthAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow render pass");
	}

	// create framebuffer
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &shadowMap.imageView;
	framebufferInfo.width = SHADOW_MAP_RESOLUTION;
	framebufferInfo.height = SHADOW_MAP_RESOLUTION;
	framebufferInfo.layers = 1;
	if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow framebuffer");
	}

	// create pipeline
	std::array<VkDescriptorSetLayout, 2> pipelineDescriptorSetLayouts{
		commonDescriptor.modelMatrix.layout,
		lightDescriptor.layout
	};
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pipelineDescriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = pipelineDescriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}

	auto vsShadowMapCode = VulkanUtils::readFile("../shaders/shadowmap_vert.spv");
	auto fsShadowMapCode = VulkanUtils::readFile("../shaders/shadowmap_frag.spv");

	VkShaderModule vsShadowMapModule = VulkanUtils::createShaderModule(device, vsShadowMapCode);
	VkShaderModule fsShadowMapModule = VulkanUtils::createShaderModule(device, fsShadowMapCode);

	VkPipelineShaderStageCreateInfo vsShadowMapStageInfo{};
	vsShadowMapStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vsShadowMapStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vsShadowMapStageInfo.module = vsShadowMapModule;
	vsShadowMapStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fsShadowMapStageInfo{};
	fsShadowMapStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fsShadowMapStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fsShadowMapStageInfo.module = fsShadowMapModule;
	fsShadowMapStageInfo.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> shadowMapStages{vsShadowMapStageInfo, fsShadowMapStageInfo};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];

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

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shadowMapStages.size());
	pipelineInfo.pStages = shadowMapStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
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

	vkDestroyShaderModule(device, fsShadowMapModule, nullptr);
	vkDestroyShaderModule(device, vsShadowMapModule, nullptr);
}
void BaseShadowRenderPass::cleanup() {
	shadowMap.cleanup(device);
	lightDescriptor.cleanup(device);
	shadowMapDescriptor.cleanup(device);
	shadowMapLight.cleanup(device);
	vkDestroySampler(device, sampler, nullptr);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyFramebuffer(device, framebuffer, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
}
void BaseShadowRenderPass::generateShadowMap(
	std::vector<VkCommandBuffer>& commandBuffers,
	uint32_t imageIndex,
	uint32_t currentFrame,
	std::vector<void*>& modelMatrixBuffersMapped,
	const std::vector<AssetData>& models,
	const Camera& camera,
	const std::vector<DirectionalLightBuffer>& directionalLights,
	GLFWwindow* window
) {

	if (currentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		resetLayout(commandBuffers[currentFrame]);
	}

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	float aspect = (float)width / height;
	updateLightMatrix(camera, directionalLights, aspect);

	// record shadow map to VkImage
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = VkExtent2D{SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION};

	VkClearValue clearValue{};
	clearValue.depthStencil.depth = 1.0f;

	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(SHADOW_MAP_RESOLUTION);
		viewport.height = static_cast<float>(SHADOW_MAP_RESOLUTION);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = VkExtent2D{SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION};
		vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);


		for(size_t i = 0; i < models.size(); ++i) {
			uint32_t offset = models[i].updateModelTransformMatrix(i, modelMatrixBuffersMapped[currentFrame]);
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
				&lightDescriptor.sets[currentFrame],
				0,
				nullptr
			);

			vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(models[i].resource.indexCount), 1, 0, 0, 0);
		}
	}
	vkCmdEndRenderPass(commandBuffers[currentFrame]);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = shadowMap.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		commandBuffers[currentFrame],
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void BaseShadowRenderPass::resetLayout(VkCommandBuffer commandBuffer) {
	VkImageMemoryBarrier resetBarrier{};
	resetBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	resetBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	resetBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	resetBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	resetBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	resetBarrier.image = shadowMap.image;
	resetBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	resetBarrier.subresourceRange.baseMipLevel = 0;
	resetBarrier.subresourceRange.levelCount = 1;
	resetBarrier.subresourceRange.baseArrayLayer = 0;
	resetBarrier.subresourceRange.layerCount = 1;
	resetBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	resetBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &resetBarrier
	);

	currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}


void BaseShadowRenderPass::updateLightMatrix(const Camera& camera, const std::vector<DirectionalLightBuffer> directionalLights, float aspect) {
	// currently just pick up the first one
	// TODO enable multi-lighting
	DirectionalLightBuffer directionalLight = directionalLights[0];
	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		// init light matrices
		glm::vec3 lightDirection = glm::normalize(directionalLight.direction);
		float backPosScale = 20.0f;
		glm::vec3 lightPos = -lightDirection * backPosScale;
		glm::mat4 lightView = glm::lookAt(
			lightPos,
			lightPos + lightDirection,
			glm::vec3(0.0f, 1.0f, 0.0f)
		);

		float halfHeight = camera.getFarPlane() * tan(camera.getFOV());
		float halfWidth = halfHeight * aspect;

		glm::mat4 lightProj = glm::ortho(
			-halfWidth, halfWidth,
			-halfHeight, halfHeight,
			camera.getNearPlane(), camera.getFarPlane()
		);

		ShadowMapLight lightUBOData{};
		lightUBOData.view = lightView;
		lightUBOData.proj = lightProj;
		lightUBOData.proj[1][1] *= -1;

		memcpy(shadowMapLight.buffersMapped[i], &lightUBOData, sizeof(ShadowMapLight));
	}
}