#include "LogoVulkan.hpp"
#include "Lz4.hpp"
#include <cstring>

namespace LogoVK {
	namespace {
		#define LOGO_VK_DEVICE_FUNCTIONS(X) \
			X(AllocateMemory) X(FreeMemory) X(MapMemory) \
			X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(BindBufferMemory) \
			X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory) \
			X(CreateImageView) X(DestroyImageView) X(CreateSampler) X(DestroySampler) \
			X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
			X(CreateDescriptorPool) X(DestroyDescriptorPool) X(AllocateDescriptorSets) X(UpdateDescriptorSets) \
			X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
			X(CreateShaderModule) X(DestroyShaderModule) X(CreateGraphicsPipelines) X(DestroyPipeline) \
			X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) \
			X(BeginCommandBuffer) X(EndCommandBuffer) X(ResetCommandBuffer) \
			X(CmdPipelineBarrier) X(CmdCopyImage) X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline) \
			X(CmdBindDescriptorSets) X(CmdSetViewport) X(CmdSetScissor) X(CmdDraw) \
			X(QueueSubmit) X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) \
			X(CreateSemaphore) X(DestroySemaphore)

		#define LOGO_VK_MEMBER(name) PFN_vk##name name;
		struct DeviceFunctions {
			LOGO_VK_DEVICE_FUNCTIONS(LOGO_VK_MEMBER)
		} vk{};
		#undef LOGO_VK_MEMBER

		#define LOGO_VK_NAME(name) "vk" #name "\0"
		constexpr char kDeviceFunctionNames[] = LOGO_VK_DEVICE_FUNCTIONS(LOGO_VK_NAME);
		#undef LOGO_VK_NAME
		constexpr size_t CountNames(const char* names, size_t size) {
			size_t count = 0;
			for (size_t i = 0; i + 1 < size; i++) if (names[i] == '\0') count++;
			return count;
		}
		static_assert(CountNames(kDeviceFunctionNames, sizeof(kDeviceFunctionNames)) == sizeof(DeviceFunctions) / sizeof(void*),
			"kDeviceFunctionNames must name every member of DeviceFunctions");
		PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;

#ifndef LOGO_VK_EXTERNAL_SOURCES
		const unsigned char vertexPacked[] = {
			#embed "../../logo/vert.spv.lz4"
		};
		const unsigned char fragmentPacked[] = {
			#embed "../../logo/frag.spv.lz4"
		};
#endif

		constexpr int SLOTS = 8;
		constexpr VkDeviceSize UBO_STRIDE = 256; // largest minUniformBufferOffsetAlignment the spec allows
		constexpr uint32_t MAX_IMAGES = 8;
		constexpr uint32_t MAX_WAITS = 16;
		constexpr VkImageSubresourceRange COLOR_RANGE = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		struct Slot {
			VkCommandBuffer cmd;
			VkFence fence;
			VkSemaphore done;
			bool pending;
		};

		struct SwapchainData {
			VkSwapchainKHR handle;
			bool usable;
			bool built;
			VkFormat format;
			VkExtent2D extent;
			uint32_t imageCount;
			VkImage images[MAX_IMAGES];
			VkImageView views[MAX_IMAGES];
			VkFramebuffer framebuffers[MAX_IMAGES];
			Logo::Region area;
			VkImage region;
			VkDeviceMemory regionMemory;
			VkImageView regionView;
			VkRenderPass renderPass;
			VkPipeline pipeline;
		};

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		bool functionsLoaded = false;
		bool failed = false;
		bool deviceBuilt = false;

		struct QueueFamily { VkQueue queue; uint32_t family; };
		QueueFamily queues[8]{};
		int queueCount = 0;
		VkQueue usedQueue = VK_NULL_HANDLE;

		VkPhysicalDeviceMemoryProperties memoryProperties{};
		VkCommandPool commandPool = VK_NULL_HANDLE;
		Slot slots[SLOTS]{};
		int currentSlot = 0;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		VkBuffer uniformBuffer = VK_NULL_HANDLE;
		VkDeviceMemory uniformMemory = VK_NULL_HANDLE;
		uint8_t* uniformCpu = nullptr;

		SwapchainData swapchain{};

		bool findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted, uint32_t* index) {
			for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
				if ((typeBits & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & wanted) == wanted) {
					*index = i;
					return true;
				}
			}
			return false;
		}

		bool allocate(const VkMemoryRequirements& req, VkMemoryPropertyFlags preferred, VkMemoryPropertyFlags required, VkDeviceMemory* memory) {
			uint32_t type = 0;
			if (!findMemoryType(req.memoryTypeBits, preferred, &type) && !findMemoryType(req.memoryTypeBits, required, &type)) return false;
			const VkMemoryAllocateInfo info{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = req.size,
				.memoryTypeIndex = type,
			};
			return vk.AllocateMemory(device, &info, nullptr, memory) == VK_SUCCESS;
		}

		void waitAll() {
			for (Slot& s : slots) {
				if (!s.pending) continue;
				vk.WaitForFences(device, 1, &s.fence, VK_TRUE, UINT64_MAX);
				s.pending = false;
			}
		}

		bool buildDevice(uint32_t queueFamily) {
			GetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

			const VkCommandPoolCreateInfo poolInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = queueFamily,
			};
			if (vk.CreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) return false;
			VkCommandBuffer cmds[SLOTS]{};
			const VkCommandBufferAllocateInfo cmdInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = SLOTS,
			};
			if (vk.AllocateCommandBuffers(device, &cmdInfo, cmds) != VK_SUCCESS) return false;
			const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			for (int i = 0; i < SLOTS; i++) {
				slots[i].cmd = cmds[i];
				if (vk.CreateFence(device, &fenceInfo, nullptr, &slots[i].fence) != VK_SUCCESS) return false;
				if (vk.CreateSemaphore(device, &semaphoreInfo, nullptr, &slots[i].done) != VK_SUCCESS) return false;
			}

			// Nearest, clamped: the shader only uses texelFetch.
			const VkSamplerCreateInfo samplerInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_NEAREST,
				.minFilter = VK_FILTER_NEAREST,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.maxAnisotropy = 1.0f,
				.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			};
			if (vk.CreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) return false;

			const VkDescriptorSetLayoutBinding bindings[2] = {
				{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
				{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
			};
			const VkDescriptorSetLayoutCreateInfo layoutInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = 2,
				.pBindings = bindings,
			};
			if (vk.CreateDescriptorSetLayout(device, &layoutInfo, nullptr, &setLayout) != VK_SUCCESS) return false;
			const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1,
				.pSetLayouts = &setLayout,
			};
			if (vk.CreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) return false;
			const VkDescriptorPoolSize sizes[2] = {
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1},
				{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
			};
			const VkDescriptorPoolCreateInfo descriptorPoolInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = 1,
				.poolSizeCount = 2,
				.pPoolSizes = sizes,
			};
			if (vk.CreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) return false;
			const VkDescriptorSetAllocateInfo setInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &setLayout,
			};
			if (vk.AllocateDescriptorSets(device, &setInfo, &descriptorSet) != VK_SUCCESS) return false;

			const VkBufferCreateInfo bufferInfo{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = UBO_STRIDE * SLOTS,
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			};
			if (vk.CreateBuffer(device, &bufferInfo, nullptr, &uniformBuffer) != VK_SUCCESS) return false;
			VkMemoryRequirements req{};
			vk.GetBufferMemoryRequirements(device, uniformBuffer, &req);
			const VkMemoryPropertyFlags hostFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			if (!allocate(req, hostFlags, hostFlags, &uniformMemory)) return false;
			if (vk.BindBufferMemory(device, uniformBuffer, uniformMemory, 0) != VK_SUCCESS) return false;
			void* mapped = nullptr;
			if (vk.MapMemory(device, uniformMemory, 0, UBO_STRIDE * SLOTS, 0, &mapped) != VK_SUCCESS || !mapped) return false;
			uniformCpu = (uint8_t*)mapped;
			return true;
		}

		void destroyDevice() {
			for (Slot& s : slots) {
				if (s.fence) vk.DestroyFence(device, s.fence, nullptr);
				if (s.done) vk.DestroySemaphore(device, s.done, nullptr);
				s = Slot{};
			}
			if (commandPool) vk.DestroyCommandPool(device, commandPool, nullptr);
			if (descriptorPool) vk.DestroyDescriptorPool(device, descriptorPool, nullptr);
			if (pipelineLayout) vk.DestroyPipelineLayout(device, pipelineLayout, nullptr);
			if (setLayout) vk.DestroyDescriptorSetLayout(device, setLayout, nullptr);
			if (sampler) vk.DestroySampler(device, sampler, nullptr);
			if (uniformBuffer) vk.DestroyBuffer(device, uniformBuffer, nullptr);
			if (uniformMemory) vk.FreeMemory(device, uniformMemory, nullptr);              // unmaps it
			commandPool = VK_NULL_HANDLE; descriptorPool = VK_NULL_HANDLE; descriptorSet = VK_NULL_HANDLE;
			pipelineLayout = VK_NULL_HANDLE; setLayout = VK_NULL_HANDLE; sampler = VK_NULL_HANDLE;
			uniformBuffer = VK_NULL_HANDLE; uniformMemory = VK_NULL_HANDLE; uniformCpu = nullptr;
			currentSlot = 0;
			deviceBuilt = false;
		}

		bool createModule(const unsigned char* packed, size_t packedSize, VkShaderModule* module) {
			size_t size = 0;
			unsigned char* code = Lz4::Unpack(packed, packedSize, &size);
			if (!code) return false;
			const VkShaderModuleCreateInfo info{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = size,
				.pCode = (const uint32_t*)code,
			};
			const bool ok = vk.CreateShaderModule(device, &info, nullptr, module) == VK_SUCCESS;
			free(code);
			return ok;
		}

		bool buildPipeline(SwapchainData& sc) {
			VkShaderModule modules[2]{};
			bool ok = createModule(vertexPacked, sizeof(vertexPacked), &modules[0]) &&
			          createModule(fragmentPacked, sizeof(fragmentPacked), &modules[1]);
			if (ok) {
				const VkPipelineShaderStageCreateInfo stages[2] = {
					{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = modules[0], .pName = "main"},
					{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = modules[1], .pName = "main"},
				};
				const VkPipelineVertexInputStateCreateInfo vertexInput{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
				const VkPipelineInputAssemblyStateCreateInfo assembly{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
					.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				};
				const VkPipelineViewportStateCreateInfo viewport{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
					.viewportCount = 1,
					.scissorCount = 1,
				};
				const VkPipelineRasterizationStateCreateInfo raster{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
					.polygonMode = VK_POLYGON_MODE_FILL,
					.cullMode = VK_CULL_MODE_NONE,
					.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
					.lineWidth = 1.0f,
				};
				const VkPipelineMultisampleStateCreateInfo multisample{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
					.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				};
				const VkPipelineColorBlendAttachmentState blendAttachment{
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
				};
				const VkPipelineColorBlendStateCreateInfo blend{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
					.attachmentCount = 1,
					.pAttachments = &blendAttachment,
				};
				const VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
				const VkPipelineDynamicStateCreateInfo dynamic{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
					.dynamicStateCount = 2,
					.pDynamicStates = dynamicStates,
				};
				const VkGraphicsPipelineCreateInfo info{
					.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					.stageCount = 2,
					.pStages = stages,
					.pVertexInputState = &vertexInput,
					.pInputAssemblyState = &assembly,
					.pViewportState = &viewport,
					.pRasterizationState = &raster,
					.pMultisampleState = &multisample,
					.pColorBlendState = &blend,
					.pDynamicState = &dynamic,
					.layout = pipelineLayout,
					.renderPass = sc.renderPass,
					.basePipelineIndex = -1,
				};
				ok = vk.CreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &sc.pipeline) == VK_SUCCESS;
			}
			if (modules[0]) vk.DestroyShaderModule(device, modules[0], nullptr);
			if (modules[1]) vk.DestroyShaderModule(device, modules[1], nullptr);
			return ok;
		}

		bool buildSwapchain(SwapchainData& sc) {
			const VkAttachmentDescription attachment{
				.format = sc.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			};
			const VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
			const VkSubpassDescription subpass{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorRef,
			};
			const VkSubpassDependency dependencies[2] = {
				{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0},
				{0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 0},
			};
			const VkRenderPassCreateInfo passInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments = &attachment,
				.subpassCount = 1,
				.pSubpasses = &subpass,
				.dependencyCount = 2,
				.pDependencies = dependencies,
			};
			if (vk.CreateRenderPass(device, &passInfo, nullptr, &sc.renderPass) != VK_SUCCESS) return false;
			if (!buildPipeline(sc)) return false;

			for (uint32_t i = 0; i < sc.imageCount; i++) {
				const VkImageViewCreateInfo viewInfo{
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = sc.images[i],
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = sc.format,
					.subresourceRange = COLOR_RANGE,
				};
				if (vk.CreateImageView(device, &viewInfo, nullptr, &sc.views[i]) != VK_SUCCESS) return false;
				const VkFramebufferCreateInfo framebufferInfo{
					.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass = sc.renderPass,
					.attachmentCount = 1,
					.pAttachments = &sc.views[i],
					.width = sc.extent.width,
					.height = sc.extent.height,
					.layers = 1,
				};
				if (vk.CreateFramebuffer(device, &framebufferInfo, nullptr, &sc.framebuffers[i]) != VK_SUCCESS) return false;
			}

			sc.area = Logo::BottomLeftRegion((int)sc.extent.width, (int)sc.extent.height, true);
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = sc.format,
				.extent = {(uint32_t)sc.area.width, (uint32_t)sc.area.height, 1},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};
			if (vk.CreateImage(device, &imageInfo, nullptr, &sc.region) != VK_SUCCESS) return false;
			VkMemoryRequirements req{};
			vk.GetImageMemoryRequirements(device, sc.region, &req);
			if (!allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, &sc.regionMemory)) return false;
			if (vk.BindImageMemory(device, sc.region, sc.regionMemory, 0) != VK_SUCCESS) return false;
			const VkImageViewCreateInfo regionViewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = sc.region,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = sc.format,
				.subresourceRange = COLOR_RANGE,
			};
			if (vk.CreateImageView(device, &regionViewInfo, nullptr, &sc.regionView) != VK_SUCCESS) return false;

			// Nothing is in flight (waitAll before building), so the set can be rewritten.
			const VkDescriptorBufferInfo bufferInfo{uniformBuffer, 0, sizeof(Logo::Params)};
			const VkDescriptorImageInfo imageDescriptor{sampler, sc.regionView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			const VkWriteDescriptorSet writes[2] = {
				{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 0, .descriptorCount = 1,
				 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .pBufferInfo = &bufferInfo},
				{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 1, .descriptorCount = 1,
				 .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &imageDescriptor},
			};
			vk.UpdateDescriptorSets(device, 2, writes, 0, nullptr);
			return true;
		}

		void destroySwapchain(SwapchainData& sc) {
			for (uint32_t i = 0; i < MAX_IMAGES; i++) {
				if (sc.framebuffers[i]) vk.DestroyFramebuffer(device, sc.framebuffers[i], nullptr);
				if (sc.views[i]) vk.DestroyImageView(device, sc.views[i], nullptr);
				sc.framebuffers[i] = VK_NULL_HANDLE;
				sc.views[i] = VK_NULL_HANDLE;
			}
			if (sc.pipeline) vk.DestroyPipeline(device, sc.pipeline, nullptr);
			if (sc.renderPass) vk.DestroyRenderPass(device, sc.renderPass, nullptr);
			if (sc.regionView) vk.DestroyImageView(device, sc.regionView, nullptr);
			if (sc.region) vk.DestroyImage(device, sc.region, nullptr);
			if (sc.regionMemory) vk.FreeMemory(device, sc.regionMemory, nullptr);
			sc.pipeline = VK_NULL_HANDLE; sc.renderPass = VK_NULL_HANDLE; sc.regionView = VK_NULL_HANDLE;
			sc.region = VK_NULL_HANDLE; sc.regionMemory = VK_NULL_HANDLE;
			sc.built = false;
		}

		uint32_t familyOf(VkQueue queue) {
			for (int i = 0; i < queueCount; i++) if (queues[i].queue == queue) return queues[i].family;
			return 0; // Switch exposes a single queue family
		}

		VkImageMemoryBarrier imageBarrier(VkImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout from, VkImageLayout to) {
			return {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = srcAccess,
				.dstAccessMask = dstAccess,
				.oldLayout = from,
				.newLayout = to,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = image,
				.subresourceRange = COLOR_RANGE,
			};
		}

		void record(VkCommandBuffer cmd, const SwapchainData& sc, uint32_t imageIndex, uint32_t dynamicOffset) {
			const VkImage image = sc.images[imageIndex];

			const VkImageMemoryBarrier toCopy[2] = {
				imageBarrier(image, 0, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
				imageBarrier(sc.region, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
			};
			vk.CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                      0, 0, nullptr, 0, nullptr, 2, toCopy);

			const VkImageCopy copy{
				.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
				.srcOffset = {sc.area.x, sc.area.y, 0},
				.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
				.dstOffset = {0, 0, 0},
				.extent = {(uint32_t)sc.area.width, (uint32_t)sc.area.height, 1},
			};
			vk.CmdCopyImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sc.region, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

			const VkImageMemoryBarrier toDraw[2] = {
				imageBarrier(image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
				imageBarrier(sc.region, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			};
			vk.CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			                      0, 0, nullptr, 0, nullptr, 2, toDraw);

			const VkRenderPassBeginInfo begin{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = sc.renderPass,
				.framebuffer = sc.framebuffers[imageIndex],
				.renderArea = {{0, 0}, sc.extent},
			};
			vk.CmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
			vk.CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sc.pipeline);
			vk.CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, &dynamicOffset);
			const VkViewport viewport{0.0f, 0.0f, (float)sc.extent.width, (float)sc.extent.height, 0.0f, 1.0f};
			const VkRect2D scissor{{0, 0}, sc.extent};
			vk.CmdSetViewport(cmd, 0, 1, &viewport);
			vk.CmdSetScissor(cmd, 0, 1, &scissor);
			vk.CmdDraw(cmd, Logo::VERTEX_COUNT, 1, 0, 0);
			vk.CmdEndRenderPass(cmd);
		}
	}

	void OnDeviceCreated(VkPhysicalDevice pd, VkDevice dev, DeviceResolver deviceResolve, InstanceResolver instanceResolve) {
		if (device && device != dev) Release();
		physicalDevice = pd;
		device = dev;
		failed = false;
		queueCount = 0;
		usedQueue = VK_NULL_HANDLE;
		swapchain = SwapchainData{};

		functionsLoaded = true;
		const char* name = kDeviceFunctionNames;
		for (size_t i = 0; i < sizeof(DeviceFunctions) / sizeof(void*); i++, name += strlen(name) + 1) {
			void* address = deviceResolve(dev, name);
			if (!address) functionsLoaded = false;
			memcpy(reinterpret_cast<char*>(&vk) + i * sizeof(void*), &address, sizeof(void*));
		}
		GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)instanceResolve("vkGetPhysicalDeviceMemoryProperties");
		GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)instanceResolve("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		if (!GetPhysicalDeviceMemoryProperties || !GetPhysicalDeviceSurfaceCapabilitiesKHR) functionsLoaded = false;
	}

	void OnDeviceQueue(VkDevice dev, uint32_t queueFamily, VkQueue queue) {
		if (dev != device || !queue) return;
		for (int i = 0; i < queueCount; i++) if (queues[i].queue == queue) return;
		if (queueCount < (int)(sizeof(queues) / sizeof(queues[0]))) queues[queueCount++] = {queue, queueFamily};
	}

	VkImageUsageFlags ExtraSwapchainUsage(VkDevice dev, VkSurfaceKHR surface) {
		if (dev != device || !functionsLoaded || failed) return 0;
		VkSurfaceCapabilitiesKHR caps{};
		if (GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps) != VK_SUCCESS) return 0;
		return caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	void OnSwapchainCreated(VkDevice dev, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR handle, uint32_t imageCount, const VkImage* images) {
		if (dev != device || !functionsLoaded) return;
		waitAll();
		destroySwapchain(swapchain);
		swapchain = SwapchainData{};
		swapchain.handle = handle;
		swapchain.format = info->imageFormat;
		swapchain.extent = info->imageExtent;
		swapchain.imageCount = imageCount;
		const VkImageUsageFlags needed = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain.usable = (info->imageUsage & needed) == needed && info->imageArrayLayers == 1 &&
		                   imageCount > 0 && imageCount <= MAX_IMAGES && info->imageExtent.width > 0 && info->imageExtent.height > 0;
		if (swapchain.usable) memcpy(swapchain.images, images, imageCount * sizeof(VkImage));
	}

	void OnSwapchainDestroyed(VkDevice dev, VkSwapchainKHR handle) {
		if (dev != device || !functionsLoaded || !handle || handle != swapchain.handle) return;
		waitAll();
		destroySwapchain(swapchain);
		swapchain = SwapchainData{};
	}

	VkSemaphore Draw(VkQueue queue, VkSwapchainKHR handle, uint32_t imageIndex, uint32_t waitCount, const VkSemaphore* waits, float time) {
		if (!functionsLoaded || failed || !device) return VK_NULL_HANDLE;
		if (!handle || handle != swapchain.handle || !swapchain.usable || imageIndex >= swapchain.imageCount || waitCount > MAX_WAITS) return VK_NULL_HANDLE;
		if (usedQueue && queue != usedQueue) return VK_NULL_HANDLE;

		if (!deviceBuilt) {
			deviceBuilt = true;
			if (!buildDevice(familyOf(queue))) { failed = true; return VK_NULL_HANDLE; }
		}
		if (!swapchain.built) {
			waitAll();
			swapchain.built = true;
			if (!buildSwapchain(swapchain)) { swapchain.usable = false; return VK_NULL_HANDLE; }
		}
		usedQueue = queue;

		Slot& slot = slots[currentSlot];
		const uint32_t dynamicOffset = (uint32_t)(currentSlot * UBO_STRIDE);
		currentSlot = (currentSlot + 1) % SLOTS;
		if (slot.pending) {
			vk.WaitForFences(device, 1, &slot.fence, VK_TRUE, UINT64_MAX);
			slot.pending = false;
		}

		Logo::Params params{};
		params.time = time;
		params.version = Logo::kVersion.packed;
		params.cropWidth = swapchain.extent.width;
		params.cropHeight = swapchain.extent.height;
		params.cropX = 0;
		params.cropY = 0;
		params.regionX = swapchain.area.x;
		params.regionY = swapchain.area.y;
		memcpy(uniformCpu + dynamicOffset, &params, sizeof(params)); // host coherent

		vk.ResetCommandBuffer(slot.cmd, 0);
		const VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		if (vk.BeginCommandBuffer(slot.cmd, &beginInfo) != VK_SUCCESS) return VK_NULL_HANDLE;
		record(slot.cmd, swapchain, imageIndex, dynamicOffset);
		if (vk.EndCommandBuffer(slot.cmd) != VK_SUCCESS) return VK_NULL_HANDLE;

		VkPipelineStageFlags waitStages[MAX_WAITS];
		for (uint32_t i = 0; i < waitCount; i++) waitStages[i] = VK_PIPELINE_STAGE_TRANSFER_BIT;
		const VkSubmitInfo submit{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = waitCount,
			.pWaitSemaphores = waits,
			.pWaitDstStageMask = waitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &slot.cmd,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &slot.done,
		};
		vk.ResetFences(device, 1, &slot.fence);
		if (vk.QueueSubmit(queue, 1, &submit, slot.fence) != VK_SUCCESS) {
			failed = true;
			return VK_NULL_HANDLE;
		}
		slot.pending = true;
		return slot.done;
	}

	void Release() {
		if (!device || !functionsLoaded) return;
		waitAll();
		destroySwapchain(swapchain);
		destroyDevice();
		swapchain.handle = VK_NULL_HANDLE;
		swapchain.usable = false;
		failed = true; // not drawn again
	}
}
