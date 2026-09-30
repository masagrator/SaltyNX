#include "LogoNVN.hpp"
#include <cstddef>
#include <cstring>

namespace LogoNVN {
	namespace {
		using namespace NVN;
		using namespace ::Logo;

		constexpr int UBO_BINDING = 0;
		constexpr int TEXTURE_BINDING = 0;
		constexpr int STAGE_VERTEX = 0;   // NVNshaderStage
		constexpr int STAGE_FRAGMENT = 1;
		constexpr int MEMORY_POOL_FLAGS_CPU_UNCACHED = 0x2;
		constexpr int MEMORY_POOL_FLAGS_GPU_CACHED = 0x20;
		constexpr int MEMORY_POOL_FLAGS_SHADER_CODE = 0x40;
		constexpr int BARRIER_ORDER_FRAGMENTS = 0x2;
		constexpr int BARRIER_INVALIDATE_TEXTURE = 0x10;

		// X(name without "nvn", return type, arguments). Loaded through the resolver in a loop.
		#define LOGO_NVN_FUNCTIONS(X) \
			X(DeviceGetInteger, void, const Device* device, int info, int* out) \
			X(DeviceGetTextureHandle, TextureHandle, const Device* device, int textureId, int samplerId) \
			X(MemoryPoolBuilderSetDefaults, void, const MemoryPoolBuilder* builder) \
			X(MemoryPoolBuilderSetDevice, void, const MemoryPoolBuilder* builder, const Device* device) \
			X(MemoryPoolBuilderSetFlags, void, const MemoryPoolBuilder* builder, int flags) \
			X(MemoryPoolBuilderSetStorage, void, const MemoryPoolBuilder* builder, void* buffer, size_t size) \
			X(MemoryPoolInitialize, bool, const MemoryPool* pool, const MemoryPoolBuilder* builder) \
			X(MemoryPoolMap, void*, const MemoryPool* pool) \
			X(MemoryPoolGetBufferAddress, BufferAddress, const MemoryPool* pool) \
			X(ProgramInitialize, bool, Program* program, Device* device) \
			X(ProgramSetShaders, bool, Program* program, int count, const ShaderData* shaders) \
			X(DepthStencilStateSetDefaults, void, DepthStencilState* state) \
			X(ChannelMaskStateSetDefaults, void, ChannelMaskState* state) \
			X(ColorStateSetDefaults, void, ColorState* state) \
			X(BlendStateSetDefaults, void, BlendState* state) \
			X(PolygonStateSetDefaults, void, PolygonState* state) \
			X(TexturePoolInitialize, bool, TexturePool* pool, const MemoryPool* memoryPool, ptrdiff_t offset, int numDescriptors) \
			X(TexturePoolRegisterTexture, void, const TexturePool* pool, int id, const Texture* texture, const TextureView* view) \
			X(SamplerPoolInitialize, bool, SamplerPool* pool, const MemoryPool* memoryPool, ptrdiff_t offset, int numDescriptors) \
			X(SamplerPoolRegisterSampler, void, const SamplerPool* pool, int id, const Sampler* sampler) \
			X(SamplerBuilderSetDevice, void, SamplerBuilder* builder, Device* device) \
			X(SamplerBuilderSetDefaults, void, SamplerBuilder* builder) \
			X(SamplerBuilderSetMinMagFilter, void, SamplerBuilder* builder, int min, int mag) \
			X(SamplerBuilderSetWrapMode, void, SamplerBuilder* builder, int s, int t, int r) \
			X(SamplerInitialize, bool, Sampler* sampler, const SamplerBuilder* builder) \
			X(CommandBufferInitialize, bool, const CommandBuffer* cmdBuf, Device* device) \
			X(CommandBufferAddCommandMemory, void, const CommandBuffer* cmdBuf, const MemoryPool* pool, ptrdiff_t offset, size_t size) \
			X(CommandBufferAddControlMemory, void, const CommandBuffer* cmdBuf, void* buffer, size_t size) \
			X(CommandBufferBeginRecording, void, const CommandBuffer* cmdBuf) \
			X(CommandBufferEndRecording, CommandHandle, const CommandBuffer* cmdBuf) \
			X(CommandBufferSetRenderTargets, void*, CommandBuffer* cmdBuf, int numTextures, const Texture** textures, const TextureView** views, const Texture* depth, const TextureView* depthView) \
			X(CommandBufferSetViewport, void*, CommandBuffer* cmdBuf, int x, int y, int width, int height) \
			X(CommandBufferBarrier, void, const CommandBuffer* cmdBuf, int barrier) \
			X(CommandBufferSetTexturePool, void, const CommandBuffer* cmdBuf, const TexturePool* pool) \
			X(CommandBufferSetSamplerPool, void, const CommandBuffer* cmdBuf, const SamplerPool* pool) \
			X(CommandBufferBindDepthStencilState, void, const CommandBuffer* cmdBuf, const DepthStencilState* state) \
			X(CommandBufferBindChannelMaskState, void, const CommandBuffer* cmdBuf, const ChannelMaskState* state) \
			X(CommandBufferBindColorState, void, const CommandBuffer* cmdBuf, const ColorState* state) \
			X(CommandBufferBindBlendState, void, const CommandBuffer* cmdBuf, const BlendState* state) \
			X(CommandBufferBindPolygonState, void, const CommandBuffer* cmdBuf, const PolygonState* state) \
			X(CommandBufferBindProgram, void, const CommandBuffer* cmdBuf, const Program* program, int stages) \
			X(CommandBufferBindUniformBuffer, void, const CommandBuffer* cmdBuf, int stage, int index, BufferAddress address, size_t size) \
			X(CommandBufferBindTexture, void, const CommandBuffer* cmdBuf, int stage, int index, TextureHandle handle) \
			X(CommandBufferDrawArrays, void, const CommandBuffer* cmdBuf, int mode, int first, int count) \
			X(QueueSubmitCommands, void, const Queue* queue, int numCommandBuffers, const CommandHandle* handles)

		#define LOGO_NVN_MEMBER(name, ret, ...) ret (*name)(__VA_ARGS__);
		struct Functions {
			LOGO_NVN_FUNCTIONS(LOGO_NVN_MEMBER)
		} nvn{};
		#undef LOGO_NVN_MEMBER

		// All names in one string ("nvnDeviceGetInteger\0..."), same order as Functions, loaded in a loop.
		#define LOGO_NVN_NAME(name, ...) "nvn" #name "\0"
		constexpr char kFunctionNames[] = LOGO_NVN_FUNCTIONS(LOGO_NVN_NAME);
		#undef LOGO_NVN_NAME
		constexpr size_t CountNames(const char* names, size_t size) {
			size_t count = 0;
			for (size_t i = 0; i + 1 < size; i++) if (names[i] == '\0') count++;
			return count;
		}
		static_assert(CountNames(kFunctionNames, sizeof(kFunctionNames)) == sizeof(Functions) / sizeof(void*),
			"kFunctionNames must name every member of Functions");

		constexpr char vertexDataStorage[] {
			#embed "../../logo/vert.code.bin"
		};
		constexpr char fragmentDataStorage[] {
			#embed "../../logo/frag.code.bin"
		};
		char vertexControlStorage[] {
			#embed "../../logo/vert.control.bin"
		};
		char fragmentControlStorage[] {
			#embed "../../logo/frag.control.bin"
		};
		// The constexpr copies above are only used for sizes and the static_asserts below (never odr-used,
		// so not emitted). The GPU gets one writable buffer that is directly the shader pool storage:
		// vertex code, zero padding to 256 (stage alignment), fragment code, zero padding to 4 KiB (pool size).
		constexpr size_t fragmentDataOffset = (sizeof(vertexDataStorage) + 0xFF) & ~0xFF;
		constexpr size_t shaderDataSize = (fragmentDataOffset + sizeof(fragmentDataStorage) + 0xFFF) & ~0xFFF;
		struct ShaderCode {
			char vertex[fragmentDataOffset];
			char fragment[shaderDataSize - fragmentDataOffset];
		};
		alignas(0x1000) ShaderCode shaderDataStorage {
			{
				#embed "../../logo/vert.code.bin"
			},
			{
				#embed "../../logo/frag.code.bin"
			},
		};
		static_assert(sizeof(ShaderCode) == shaderDataSize && offsetof(ShaderCode, fragment) == fragmentDataOffset);

		// Build-time guard: the logo must not need shader scratch memory.
		constexpr uint32_t ReadU32(const char* p) {
			return (uint32_t)(unsigned char)p[0] | ((uint32_t)(unsigned char)p[1] << 8) |
			       ((uint32_t)(unsigned char)p[2] << 16) | ((uint32_t)(unsigned char)p[3] << 24);
		}
		// Returns 0 = no local memory, 1 = needs scratch memory, 2 = SPH not found.
		constexpr int SphLocalMemoryState(const char* data, size_t size, uint32_t sphType, uint32_t shaderType) {
			for (size_t off = 0; off + 0x50 <= size && off <= 0x100; off += 4) {
				const uint32_t w0 = ReadU32(data + off);
				if ((w0 & 0x1F) != sphType || ((w0 >> 5) & 0x1F) != 3 || ((w0 >> 10) & 0xF) != shaderType) continue;
				return ((ReadU32(data + off + 4) | ReadU32(data + off + 8) | ReadU32(data + off + 12)) & 0xFFFFFF) ? 1 : 0;
			}
			return 2;
		}
		static_assert(SphLocalMemoryState(vertexDataStorage, sizeof(vertexDataStorage), 1, 1) != 1,
			"saltynx.vert needs shader scratch memory (local memory / CRS spill), remove divergent break/continue/early returns");
		static_assert(SphLocalMemoryState(fragmentDataStorage, sizeof(fragmentDataStorage), 2, 5) != 1,
			"saltynx.frag needs shader scratch memory (local memory / CRS spill)");

		MemoryPool shaderPool{};
		Program program{};
		MemoryPool cmdMemPool{};
		// Ring of command/control memory slots. The GPU may still be reading a previous
		// frame's commands, so every frame records into the next slot instead of overwriting.
		constexpr int SLOTS = 8;
		constexpr size_t SLOT_SIZE = 0x1000;
		alignas(0x1000) char cmdHostStorage[SLOTS * SLOT_SIZE]{};
		alignas(0x1000) char controlMemStorage[SLOTS * SLOT_SIZE]{};
		int currentSlot = 0;
		CommandBuffer cmdBuf{};

		DepthStencilState depthState{};
		ChannelMaskState channelState{};
		ColorState colorState{};
		BlendState blendState{};
		PolygonState polygonState{};

		constexpr size_t TEX_POOL_OFFSET = 0x0000;
		constexpr size_t SAMPLER_POOL_OFFSET = 0x4000;
		constexpr size_t UBO_POOL_OFFSET = 0x8000;
		constexpr size_t RES_POOL_SIZE = 0x9000;
		alignas(0x1000) char resHostStorage[RES_POOL_SIZE]{};
		MemoryPool resPool{};
		TexturePool texturePool{};
		SamplerPool samplerPool{};
		Sampler framebufferSampler{};
		uint8_t* uboCpu = nullptr;
		BufferAddress uboGpu = 0;
		size_t uboStride = 0x100;          // per slot, >= device uniform buffer alignment
		int reservedTextureDescriptors = 256;
		int reservedSamplerDescriptors = 256;

		bool initAttempted = false;
		bool ready = false;

		bool loadFunctions(Resolver resolve) {
			const char* name = kFunctionNames;
			for (size_t i = 0; i < sizeof(Functions) / sizeof(void*); i++, name += strlen(name) + 1) {
				void* address = resolve(name);
				if (!address) return false;
				memcpy(reinterpret_cast<char*>(&nvn) + i * sizeof(void*), &address, sizeof(void*));
			}
			return true;
		}

		bool initPool(MemoryPool* pool, Device* device, int flags, void* storage, size_t size) {
			MemoryPoolBuilder b{};
			nvn.MemoryPoolBuilderSetDefaults(&b);
			nvn.MemoryPoolBuilderSetDevice(&b, device);
			nvn.MemoryPoolBuilderSetFlags(&b, flags);
			nvn.MemoryPoolBuilderSetStorage(&b, storage, size);
			return nvn.MemoryPoolInitialize(pool, &b);
		}

		bool buildResources(Device* device) {
			// NVNdeviceInfo: 4 = UNIFORM_BUFFER_ALIGNMENT, 15/16 = TEXTURE/SAMPLER_DESCRIPTOR_SIZE,
			// 17/18 = RESERVED_TEXTURE/SAMPLER_DESCRIPTORS. Ids below the reserved counts belong to NVN,
			// ours have to come after them.
			int uboAlign = 0x100, texDescSize = 0x20, samplerDescSize = 0x20;
			nvn.DeviceGetInteger(device, 4, &uboAlign);
			nvn.DeviceGetInteger(device, 15, &texDescSize);
			nvn.DeviceGetInteger(device, 16, &samplerDescSize);
			nvn.DeviceGetInteger(device, 17, &reservedTextureDescriptors);
			nvn.DeviceGetInteger(device, 18, &reservedSamplerDescriptors);
			if (uboAlign < 0x100) uboAlign = 0x100;
			uboStride = (size_t)uboAlign;

			const size_t texPoolBytes = (size_t)(reservedTextureDescriptors + SLOTS) * (size_t)texDescSize;
			const size_t samplerPoolBytes = (size_t)(reservedSamplerDescriptors + 1) * (size_t)samplerDescSize;
			if (texPoolBytes > SAMPLER_POOL_OFFSET - TEX_POOL_OFFSET) return false;
			if (samplerPoolBytes > UBO_POOL_OFFSET - SAMPLER_POOL_OFFSET) return false;
			if (uboStride * SLOTS > RES_POOL_SIZE - UBO_POOL_OFFSET) return false;

			if (!initPool(&resPool, device, MEMORY_POOL_FLAGS_CPU_UNCACHED | MEMORY_POOL_FLAGS_GPU_CACHED, resHostStorage, sizeof(resHostStorage))) return false;

			uboCpu = (uint8_t*)nvn.MemoryPoolMap(&resPool);
			if (!uboCpu) return false;
			uboCpu += UBO_POOL_OFFSET;
			uboGpu = nvn.MemoryPoolGetBufferAddress(&resPool) + UBO_POOL_OFFSET;

			SamplerBuilder sb{};
			nvn.SamplerBuilderSetDefaults(&sb);
			nvn.SamplerBuilderSetDevice(&sb, device);
			nvn.SamplerBuilderSetMinMagFilter(&sb, 0, 0);
			nvn.SamplerBuilderSetWrapMode(&sb, 0, 0, 0);
			if (!nvn.SamplerInitialize(&framebufferSampler, &sb)) return false;

			if (!nvn.SamplerPoolInitialize(&samplerPool, &resPool, (ptrdiff_t)SAMPLER_POOL_OFFSET, reservedSamplerDescriptors + 1)) return false;
			nvn.SamplerPoolRegisterSampler(&samplerPool, reservedSamplerDescriptors, &framebufferSampler);

			return nvn.TexturePoolInitialize(&texturePool, &resPool, (ptrdiff_t)TEX_POOL_OFFSET, reservedTextureDescriptors + SLOTS);
		}

		bool buildProgram(Device* device) {
			if (!initPool(&shaderPool, device, MEMORY_POOL_FLAGS_CPU_UNCACHED | MEMORY_POOL_FLAGS_GPU_CACHED | MEMORY_POOL_FLAGS_SHADER_CODE,
			              &shaderDataStorage, sizeof(shaderDataStorage))) return false;

			const BufferAddress base = nvn.MemoryPoolGetBufferAddress(&shaderPool);
			if (!nvn.ProgramInitialize(&program, device)) return false;

			const ShaderData stages[2] = {
				{ base, vertexControlStorage },
				{ base + fragmentDataOffset, fragmentControlStorage },
			};
			if (!nvn.ProgramSetShaders(&program, 2, stages)) return false;

			nvn.DepthStencilStateSetDefaults(&depthState);
			nvn.ChannelMaskStateSetDefaults(&channelState);
			nvn.ColorStateSetDefaults(&colorState);
			nvn.BlendStateSetDefaults(&blendState);
			nvn.PolygonStateSetDefaults(&polygonState);
			return true;
		}

		bool init(Device* device, Resolver resolve) {
			if (initAttempted) return ready;
			initAttempted = true;
			const char* failed = nullptr;
			if (!device || !loadFunctions(resolve)) failed = "functions";
			else if (!initPool(&cmdMemPool, device, MEMORY_POOL_FLAGS_CPU_UNCACHED | MEMORY_POOL_FLAGS_GPU_CACHED, cmdHostStorage, sizeof(cmdHostStorage))
			         || !nvn.CommandBufferInitialize(&cmdBuf, device)) failed = "command buffer";
			else if (!buildResources(device)) failed = "resources";
			else if (!buildProgram(device)) failed = "program";
			ready = !failed;
			if (ready) SaltySDCore_printf("NX-FPS: Logo: NVN ready (vert 0x%lX, frag 0x%lX)\n", (unsigned long)sizeof(vertexDataStorage), (unsigned long)sizeof(fragmentDataStorage));
			else SaltySDCore_printf("NX-FPS: Logo: NVN init failed: %s\n", failed);
			return ready;
		}
	}

	bool Draw(Device* device, const Queue* queue, const Texture* target, int width, int height,
	          const Rectangle& crop, float time,
	          const TexturePool* gameTexturePool, const SamplerPool* gameSamplerPool, Resolver resolve) {
		if (!init(device, resolve)) return false;
		if (!target || width <= 0 || height <= 0) return true;

		const bool haveCrop = crop.width > 0 && crop.height > 0;
		const int visibleW = haveCrop ? crop.width : width;
		const int visibleH = haveCrop ? crop.height : height;
		const int originX = haveCrop ? crop.x : 0;
		const int originY = haveCrop ? crop.y : 0;

		const int slot = currentSlot;
		nvn.CommandBufferAddCommandMemory(&cmdBuf, &cmdMemPool, slot * SLOT_SIZE, SLOT_SIZE);
		nvn.CommandBufferAddControlMemory(&cmdBuf, controlMemStorage + slot * SLOT_SIZE, SLOT_SIZE);
		currentSlot = (currentSlot + 1) % SLOTS;

		// Per-slot UBO: the GPU may still be reading the previous slots' data.
		Params params{};
		params.time = time;
		params.version = kVersion.packed;
		params.cropWidth = (uint32_t)visibleW;
		params.cropHeight = (uint32_t)visibleH;
		params.cropX = (uint32_t)originX;
		params.cropY = (uint32_t)originY;
		params.regionX = 0; // NVN samples the frame itself
		params.regionY = 0;
		memcpy(uboCpu + slot * uboStride, &params, sizeof(params));
		const BufferAddress paramsAddress = uboGpu + slot * uboStride;

		const int textureId = reservedTextureDescriptors + slot;
		nvn.TexturePoolRegisterTexture(&texturePool, textureId, target, nullptr);
		const TextureHandle framebufferHandle = nvn.DeviceGetTextureHandle(device, textureId, reservedSamplerDescriptors);

		nvn.CommandBufferBeginRecording(&cmdBuf);
		nvn.CommandBufferSetRenderTargets(&cmdBuf, 1, &target, nullptr, nullptr, nullptr);
		nvn.CommandBufferSetViewport(&cmdBuf, originX, originY, visibleW, visibleH);
		nvn.CommandBufferBarrier(&cmdBuf, BARRIER_ORDER_FRAGMENTS | BARRIER_INVALIDATE_TEXTURE);
		nvn.CommandBufferSetTexturePool(&cmdBuf, &texturePool);
		nvn.CommandBufferSetSamplerPool(&cmdBuf, &samplerPool);
		nvn.CommandBufferBindDepthStencilState(&cmdBuf, &depthState);
		nvn.CommandBufferBindChannelMaskState(&cmdBuf, &channelState);
		nvn.CommandBufferBindColorState(&cmdBuf, &colorState);
		nvn.CommandBufferBindBlendState(&cmdBuf, &blendState);
		nvn.CommandBufferBindPolygonState(&cmdBuf, &polygonState);
		nvn.CommandBufferBindProgram(&cmdBuf, &program, 0x1F);
		nvn.CommandBufferBindUniformBuffer(&cmdBuf, STAGE_VERTEX, UBO_BINDING, paramsAddress, sizeof(Params));
		nvn.CommandBufferBindUniformBuffer(&cmdBuf, STAGE_FRAGMENT, UBO_BINDING, paramsAddress, sizeof(Params));
		nvn.CommandBufferBindTexture(&cmdBuf, STAGE_FRAGMENT, TEXTURE_BINDING, framebufferHandle);
		nvn.CommandBufferDrawArrays(&cmdBuf, 4 /*TRIANGLES*/, 0, VERTEX_COUNT);
		// Give the queue back the pools the game expects to still be bound (none recorded yet = nothing to restore).
		if (gameTexturePool) nvn.CommandBufferSetTexturePool(&cmdBuf, gameTexturePool);
		if (gameSamplerPool) nvn.CommandBufferSetSamplerPool(&cmdBuf, gameSamplerPool);
		const CommandHandle handle = nvn.CommandBufferEndRecording(&cmdBuf);
		nvn.QueueSubmitCommands(queue, 1, &handle);
		return true;
	}
}
