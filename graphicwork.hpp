/*
* *
* * Copyright (C) 2020 Samsung Electronics
* *
* */


#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <iostream>
#include <algorithm>
#include <ctime>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanTools.h"
#include "base.hpp"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
//android_app* androidapp;
#endif

#define DEBUG (!NDEBUG)

#define BUFFER_ELEMENTS 32

class GraphicsWork : public Workload
{
public:
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	uint32_t queueFamilyIndex;
	VkPipelineCache pipelineCache;
	VkQueue queue;
    VkFence fence;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	std::vector<VkShaderModule> shaderModules;
	VkBuffer vertexBuffer, indexBuffer;
	VkDeviceMemory vertexMemory, indexMemory;
	VkQueryPool query_pool;

	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	};
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment colorAttachment, depthAttachment;
	VkRenderPass renderPass;

	VkDebugReportCallbackEXT debugReportCallback{};

	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
		for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
			if ((typeBits & 1) == 1) {
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}
			typeBits >>= 1;
		}
		return 0;
	}

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer *buffer, VkDeviceMemory *memory, VkDeviceSize size, void *data = nullptr)
	{
		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(usageFlags, size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(device, *buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, memory));

		if (data != nullptr) {
			void *mapped;
			VK_CHECK_RESULT(vkMapMemory(device, *memory, 0, size, 0, &mapped));
			memcpy(mapped, data, size);
			vkUnmapMemory(device, *memory);
		}

		VK_CHECK_RESULT(vkBindBufferMemory(device, *buffer, *memory, 0));

		return VK_SUCCESS;
	}

	/*
		Submit command buffer to a queue and wait for fence until queue operations have been finished
	*/
	void submitWork(VkCommandBuffer cmdBuffer, VkQueue queue)
	{
		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;
		VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();

		VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
		VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
		vkDestroyFence(device, fence, nullptr);
	}

	GraphicsWork(Base& base, QueueInfo queueInfo, unsigned commandCount = 10, unsigned triangleCount = 3)
	{
        device = base.GetDevice();
        instance = base.GetInstance();
        physicalDevice = base.GetPhysicalDevice();
        queueFamilyIndex = queueInfo.familyIndex;
        queue = queueInfo.queue;

		// Command pool
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool));

		VkQueryPoolCreateInfo query_pool_info;
		query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		query_pool_info.pNext = NULL;
		query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		query_pool_info.flags = 0;
		query_pool_info.queryCount = 2;
		query_pool_info.pipelineStatistics = 0;
		VK_CHECK_RESULT(vkCreateQueryPool(device, &query_pool_info, NULL, &query_pool));

		/*
			Prepare vertex and index buffers
		*/
		struct Vertex {
			float position[3];
			float color[3];
		};
        auto randRange = [](float a, float b) -> float {
            assert(b > a);
            float dist = b - a;

            return ((float)rand() / RAND_MAX * dist) - (dist/2);
        };

        std::srand(std::time(nullptr));
		{

			std::vector<Vertex> vertices = {
				{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
				{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
				{ {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
			};
			std::vector<uint32_t> indices = { 0, 1, 2 };

			const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
			const VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			// Command buffer for copy commands (reused)
			VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VkCommandBuffer copyCmd;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &copyCmd));
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

			// Copy input data to VRAM using a staging buffer
			{
				// Vertices
				createBuffer(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&stagingBuffer,
					&stagingMemory,
					vertexBufferSize,
					vertices.data());

				createBuffer(
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					&vertexBuffer,
					&vertexMemory,
					vertexBufferSize);

				VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));
				VkBufferCopy copyRegion = {};
				copyRegion.size = vertexBufferSize;
				vkCmdCopyBuffer(copyCmd, stagingBuffer, vertexBuffer, 1, &copyRegion);
				VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

				submitWork(copyCmd, queue);

				vkDestroyBuffer(device, stagingBuffer, nullptr);
				vkFreeMemory(device, stagingMemory, nullptr);

				// Indices
				createBuffer(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&stagingBuffer,
					&stagingMemory,
					indexBufferSize,
					indices.data());

				createBuffer(
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					&indexBuffer,
					&indexMemory,
					indexBufferSize);

				VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));
				copyRegion.size = indexBufferSize;
				vkCmdCopyBuffer(copyCmd, stagingBuffer, indexBuffer, 1, &copyRegion);
				VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

				submitWork(copyCmd, queue);

				vkDestroyBuffer(device, stagingBuffer, nullptr);
				vkFreeMemory(device, stagingMemory, nullptr);
			}
		}

		/*
			Create framebuffer attachments
		*/
		width = 1024;
		height = 1024;
		VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
		VkFormat depthFormat;
		vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
		{
			// Color attachment
			VkImageCreateInfo image = vks::initializers::imageCreateInfo();
			image.imageType = VK_IMAGE_TYPE_2D;
			image.format = colorFormat;
			image.extent.width = width;
			image.extent.height = height;
			image.extent.depth = 1;
			image.mipLevels = 1;
			image.arrayLayers = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;

			VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &colorAttachment.image));
			vkGetImageMemoryRequirements(device, colorAttachment.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &colorAttachment.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, colorAttachment.image, colorAttachment.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = colorFormat;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = colorAttachment.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &colorAttachment.view));

			// Depth stencil attachment
			image.format = depthFormat;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depthAttachment.image));
			vkGetImageMemoryRequirements(device, depthAttachment.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &depthAttachment.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, depthAttachment.image, depthAttachment.memory, 0));

			VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
			depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			depthStencilView.format = depthFormat;
			depthStencilView.flags = 0;
			depthStencilView.subresourceRange = {};
			depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			depthStencilView.subresourceRange.baseMipLevel = 0;
			depthStencilView.subresourceRange.levelCount = 1;
			depthStencilView.subresourceRange.baseArrayLayer = 0;
			depthStencilView.subresourceRange.layerCount = 1;
			depthStencilView.image = depthAttachment.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depthAttachment.view));
		}

		/*
			Create renderpass
		*/
		{
			std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
			// Color attachment
			attchmentDescriptions[0].format = colorFormat;
			attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			// Depth attachment
			attchmentDescriptions[1].format = depthFormat;
			attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
			subpassDescription.pDepthStencilAttachment = &depthReference;

			// Use subpass dependencies for layout transitions
			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Create the actual renderpass
			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
			renderPassInfo.pAttachments = attchmentDescriptions.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;
			renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

			VkImageView attachments[2];
			attachments[0] = colorAttachment.view;
			attachments[1] = depthAttachment.view;

			VkFramebufferCreateInfo framebufferCreateInfo = vks::initializers::framebufferCreateInfo();
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = 2;
			framebufferCreateInfo.pAttachments = attachments;
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer));
		}

		/*
			Prepare graphics pipeline
		*/
		{
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
			VkDescriptorSetLayoutCreateInfo descriptorLayout =
				vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
				vks::initializers::pipelineLayoutCreateInfo(nullptr, 0);

			// MVP via push constant block
			VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

			VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

			// Create pipeline
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
				vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

			VkPipelineRasterizationStateCreateInfo rasterizationState =
				vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);

			VkPipelineColorBlendAttachmentState blendAttachmentState =
				vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

			VkPipelineColorBlendStateCreateInfo colorBlendState =
				vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

			VkPipelineDepthStencilStateCreateInfo depthStencilState =
				vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

			VkPipelineViewportStateCreateInfo viewportState =
				vks::initializers::pipelineViewportStateCreateInfo(1, 1);

			VkPipelineMultisampleStateCreateInfo multisampleState =
				vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

			std::vector<VkDynamicState> dynamicStateEnables = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			};
			VkPipelineDynamicStateCreateInfo dynamicState =
				vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

			VkGraphicsPipelineCreateInfo pipelineCreateInfo =
				vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.pDynamicState = &dynamicState;
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineCreateInfo.pStages = shaderStages.data();

			// Vertex bindings an attributes
			// Binding description
			std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
				vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
			};

			// Attribute descriptions
			std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
				vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Position
				vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Color
			};

			VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
			vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
			vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
			vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
			vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

			pipelineCreateInfo.pVertexInputState = &vertexInputState;

			shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderStages[0].pName = "main";
			shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderStages[1].pName = "main";
#if 1
			const uint32_t triangle_vert[] = {
				#include "triangle.vert.inc"
			};
			const uint32_t triangle_frag[] = {
				#include "triangle.frag.inc"
			};
			shaderStages[0].module = vks::tools::loadShader(sizeof(triangle_vert), triangle_vert, device);
			shaderStages[1].module = vks::tools::loadShader(sizeof(triangle_frag), triangle_frag, device);
#else
			shaderStages[0].module = vks::tools::loadShader(ASSET_PATH "shaders/renderheadless/triangle.vert.spv", device);
			shaderStages[1].module = vks::tools::loadShader(ASSET_PATH "shaders/renderheadless/triangle.frag.spv", device);
#endif
			shaderModules = { shaderStages[0].module, shaderStages[1].module };
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
		}

		/*
			Command buffer creation
		*/
		{
			VkCommandBufferAllocateInfo cmdBufAllocateInfo =
				vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer));

			VkCommandBufferBeginInfo cmdBufInfo =
				vks::initializers::commandBufferBeginInfo();

			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

			vkCmdResetQueryPool(commandBuffer, query_pool, 0, 2);

			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0);

			VkClearValue clearValues[2];
			clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
			clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffer;

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {};
			viewport.height = (float)height;
			viewport.width = (float)width;
			viewport.minDepth = (float)0.0f;
			viewport.maxDepth = (float)1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			// Render scene
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            std::srand(std::time(nullptr));
			std::vector<glm::vec3> pos(commandCount);
            for (auto& v : pos) {
                float x = ((float)rand() / RAND_MAX * 3.0) - 1.5f; // [-1.5,  1.5]
                float y = ((float)rand() / RAND_MAX )      - 0.5f; // [-0.5,  0.5]
                float z = ((float)rand() / RAND_MAX * 1.5) - 4.0f; // [-4.0, -2.5]

				v = glm::vec3(x, y, z);
            }

			for (auto v : pos) {
				glm::mat4 mvpMatrix = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f) * glm::translate(glm::mat4(1.0f), v);
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvpMatrix), &mvpMatrix);

                vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffer);
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1);

			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
		}
	}

    virtual VkFence submit() override {
		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();

		VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));

        return fence;

    }

	virtual void queryTimestamp(uint64_t time_stamp[], int count) override {
		VK_CHECK_RESULT(vkGetQueryPoolResults(device, query_pool, 0, count,
			sizeof(uint64_t)*count, time_stamp, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));
	}

    virtual void waitIdle() override {
		VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
		vkDestroyFence(device, fence, nullptr);

        vkDeviceWaitIdle(device);

		/*
			Copy framebuffer image to host visible image
		*/
		const char* imagedata;
		{
			// Create the linear tiled destination image to copy to and to read the memory from
			VkImageCreateInfo imgCreateInfo(vks::initializers::imageCreateInfo());
			imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			imgCreateInfo.extent.width = width;
			imgCreateInfo.extent.height = height;
			imgCreateInfo.extent.depth = 1;
			imgCreateInfo.arrayLayers = 1;
			imgCreateInfo.mipLevels = 1;
			imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
			imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			// Create the image
			VkImage dstImage;
			VK_CHECK_RESULT(vkCreateImage(device, &imgCreateInfo, nullptr, &dstImage));
			// Create memory to back up the image
			VkMemoryRequirements memRequirements;
			VkMemoryAllocateInfo memAllocInfo(vks::initializers::memoryAllocateInfo());
			VkDeviceMemory dstImageMemory;
			vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
			memAllocInfo.allocationSize = memRequirements.size;
			// Memory must be host visible to copy from
			memAllocInfo.memoryTypeIndex = getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device, dstImage, dstImageMemory, 0));

			// Do the actual blit from the offscreen image to our host visible destination image
			VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VkCommandBuffer copyCmd;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &copyCmd));
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

			// Transition destination image to transfer destination layout
			vks::tools::insertImageMemoryBarrier(
				copyCmd,
				dstImage,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			// colorAttachment.image is already in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, and does not need to be transitioned

			VkImageCopy imageCopyRegion{};
			imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageCopyRegion.srcSubresource.layerCount = 1;
			imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageCopyRegion.dstSubresource.layerCount = 1;
			imageCopyRegion.extent.width = width;
			imageCopyRegion.extent.height = height;
			imageCopyRegion.extent.depth = 1;

			vkCmdCopyImage(
				copyCmd,
				colorAttachment.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageCopyRegion);

			// Transition destination image to general layout, which is the required layout for mapping the image memory later on
			vks::tools::insertImageMemoryBarrier(
				copyCmd,
				dstImage,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_MEMORY_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

			submitWork(copyCmd, queue);

			// Get layout of the image (including row pitch)
			VkImageSubresource subResource{};
			subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			VkSubresourceLayout subResourceLayout;

			vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

			// Map image memory so we can start copying from it
			vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&imagedata);
			imagedata += subResourceLayout.offset;

		/*
			Save host visible framebuffer image to disk (ppm format)
		*/

#if defined (VK_USE_PLATFORM_ANDROID_KHR)
			const char* filename = strcat(getenv("EXTERNAL_STORAGE"), "/headless.ppm");
#else
			const char* filename = "headless.ppm";
#endif
			std::ofstream file(filename, std::ios::out | std::ios::binary);

			// ppm header
			file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

			// If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
			// Check if source is BGR and needs swizzle
			std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
			const bool colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), VK_FORMAT_R8G8B8A8_UNORM) != formatsBGR.end());

			// ppm binary pixel data
			for (int32_t y = 0; y < height; y++) {
				unsigned int *row = (unsigned int*)imagedata;
				for (int32_t x = 0; x < width; x++) {
					if (colorSwizzle) {
						file.write((char*)row + 2, 1);
						file.write((char*)row + 1, 1);
						file.write((char*)row, 1);
					}
					else {
						file.write((char*)row, 3);
					}
					row++;
				}
				imagedata += subResourceLayout.rowPitch;
			}
			file.close();

			LOG("Framebuffer image saved to %s\n", filename);

			// Clean up resources
			vkUnmapMemory(device, dstImageMemory);
			vkFreeMemory(device, dstImageMemory, nullptr);
			vkDestroyImage(device, dstImage, nullptr);
		}

		vkQueueWaitIdle(queue);
    }

	~GraphicsWork()
	{
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexMemory, nullptr);
		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexMemory, nullptr);
		vkDestroyImageView(device, colorAttachment.view, nullptr);
		vkDestroyImage(device, colorAttachment.image, nullptr);
		vkFreeMemory(device, colorAttachment.memory, nullptr);
		vkDestroyImageView(device, depthAttachment.view, nullptr);
		vkDestroyImage(device, depthAttachment.image, nullptr);
		vkFreeMemory(device, depthAttachment.memory, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineCache(device, pipelineCache, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);
		for (auto shadermodule : shaderModules) {
			vkDestroyShaderModule(device, shadermodule, nullptr);
		}
#if DEBUG
		if (debugReportCallback) {
			PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
			assert(vkDestroyDebugReportCallback);
			vkDestroyDebugReportCallback(instance, debugReportCallback, nullptr);
		}
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		vks::android::freeVulkanLibrary();
#endif
	}
};

