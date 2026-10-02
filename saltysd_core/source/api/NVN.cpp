#include "NVN.hpp"
#include "LogoNVN.hpp"

static bool setNumActiveTexturesDetected = false;
static uint8_t amountOfAvailableBuffers = 0;
static uint64_t startFrameTick = 0;

enum {
	ZeroSyncType_None,
	ZeroSyncType_Soft,
	ZeroSyncType_Semi
};

namespace NVN {

	Sync* WindowSync = 0;
	Device* mainDevice = 0;

	static u16 (*nvnTextureGetWidth_0)(const Texture* texture);
	static u16 (*nvnTextureGetHeight_0)(const Texture* texture);
	static u32 (*nvnTextureGetFormat_0)(const Texture* texture);
	static void* (*nvnCommandBufferSetRenderTargets_0)(CommandBuffer* cmdBuf, int numTextures, const Texture** texture, const TextureView** textureView, const Texture* depth, const TextureView* depthView);
	static void* (*nvnCommandBufferSetViewport_0)(CommandBuffer* cmdBuf, int x, int y, int width, int height);
	static void* (*nvnCommandBufferSetViewports_0)(CommandBuffer* cmdBuf, int start, int count, const Viewport* viewports);
	static void* (*nvnCommandBufferSetScissor_0)(CommandBuffer* cmdBuf, int x, int y, int width, int height);
	static void* (*nvnCommandBufferSetScissors_0)(CommandBuffer* cmdBuf, int start, int count, const Scissor* viewports);
	static void (*nvnQueuePresentTexture_0)(const Queue* queue, const Window* nvnWindow, int index);
	static uintptr_t (*nvnDeviceGetProcAddress_0)(const void* unk1_a, const char* nvnFunction_a);
	static void (*nvnWindowBuilderSetTextures_0)(const WindowBuilder* nvnWindowBuilder, int buffers, const Texture** texturesBuffer);
	static void (*nvnWindowSetNumActiveTextures_0)(const Window* nvnWindow, int buffers);
	static int (*nvnWindowGetNumActiveTextures_0)(const Window* nvnWindow);
	static bool (*nvnWindowInitialize_0)(const Window* nvnWindow, WindowBuilder* windowBuilder);
	static void (*nvnWindowFinalize_0)(const Window* nvnWindow);
	static void (*nvnWindowBuilderSetNumActiveTextures_0)(const WindowBuilder* builder, int numActiveTextures);
	static void (*nvnTextureFinalize_0)(const Texture* texture);
	static Result (*nvnWindowAcquireTexture_0)(const Window* nvnWindow, const Sync* nvnSync, const int* index);
	static int (*nvnQueueAcquireTexture_0)(const Queue* queue, const Window* nvnWindow, int* index);
	static void (*nvnWindowSetPresentInterval_0)(const Window* nvnWindow, int mode);
	static int (*nvnWindowGetPresentInterval_0)(const Window* nvnWindow);
	static int (*nvnSyncWait_0)(const Sync* _this, uint64_t timeout_ns);
	static void (*nvnQueueFinish_0)(const Queue* nvnQueue);
	static void (*nvnQueueFenceSync_0)(const Queue* nvnQueue, const Sync* nvnSync, int condition, int flags);
	static void (*nvnQueueSubmitCommands_0)(const Queue* nvnQueue, int numCommandBuffers, const CommandHandle* handles);
	static bool (*nvnCommandBufferInitialize_0)(const CommandBuffer* nvnCmdBuf, Device* nvnDevice);
	static void (*nvnCommandBufferAddCommandMemory_0)(const CommandBuffer* nvnCmdBuf, const MemoryPool* nvnMemPool, ptrdiff_t offset, size_t size);
	static void (*nvnCommandBufferAddControlMemory_0)(const CommandBuffer* nvnCmdBuf, void* buffer, size_t size);
	static void (*nvnCommandBufferBeginRecording_0)(const CommandBuffer* nvnCmdBuf);
	static CommandHandle (*nvnCommandBufferEndRecording_0)(const CommandBuffer* nvnCmdBuf);
	static void (*nvnCommandBufferReportCounter_0)(const CommandBuffer* nvnCmdBuf, int type, const BufferAddress address);
	static void (*nvnCommandBufferResetCounter_0)(const CommandBuffer* nvnCmdBuf, int type);
	static bool (*nvnSyncInitialize_0)(const Sync* nvnSync, Device* nvnDevice);
	static void (*nvnMemoryPoolBuilderSetDefaults_0)(const MemoryPoolBuilder* nvnMemPoolBuilder);
	static void (*nvnMemoryPoolBuilderSetDevice_0)(const MemoryPoolBuilder* nvnMemPoolBuilder, const Device* nvnDevice);
	static void (*nvnMemoryPoolBuilderSetFlags_0)(const MemoryPoolBuilder* nvnMemPoolBuilder, int flags);
	static void (*nvnMemoryPoolBuilderSetStorage_0)(const MemoryPoolBuilder* nvnMemPoolBuilder, void* buffer, size_t size);
	static bool (*nvnMemoryPoolInitialize_0)(const MemoryPool* nvnMemPool, const MemoryPoolBuilder* nvnMemPoolBuilder);
	static void* (*nvnMemoryPoolMap_0)(const MemoryPool* nvnMemPool);
	static BufferAddress (*nvnMemoryPoolGetBufferAddress_0)(const MemoryPool* nvnMemPool);
	static bool (*nvnDeviceInitialize_0)(Device* nvnDevice, const DeviceBuilder* nvnDeviceBuilder);
	static void (*nvnTextureBuilderSetDevice_0)(TextureBuilder* builder, Device* device);
	static void (*nvnTextureBuilderSetDefaults_0)(TextureBuilder* builder);
	static void (*nvnTextureBuilderSetFlags_0)(TextureBuilder* builder, int flags);
	static void (*nvnTextureBuilderSetTarget_0)(TextureBuilder* builder, int target);
	static void (*nvnTextureBuilderSetFormat_0)(TextureBuilder* builder, int format);
	static void (*nvnTextureBuilderSetSize2D_0)(TextureBuilder* builder, int width, int height);
	static size_t (*nvnTextureBuilderGetStorageSize_0)(const TextureBuilder* builder);
	static size_t (*nvnTextureBuilderGetStorageAlignment_0)(const TextureBuilder* builder);
	static void (*nvnTextureBuilderSetStorage_0)(TextureBuilder* builder, const MemoryPool* pool, ptrdiff_t offset);
	static bool (*nvnTextureInitialize_0)(Texture* texture, const TextureBuilder* builder);
	static int (*nvnTextureGetFlags_0)(const Texture* texture);
	static int (*nvnTextureGetTarget_0)(const Texture* texture);
	static void (*nvnCommandBufferFinalize_0)(const CommandBuffer* nvnCmdBuf);
	static void (*nvnQueueWaitSync_0)(const Queue* nvnQueue, const Sync* nvnSync);
	static void (*nvnCommandBufferBarrier_0)(const CommandBuffer* nvnCmdBuf, int barrier);
	static void (*nvnCommandBufferCopyTextureToTexture_0)(const CommandBuffer* nvnCmdBuf, const Texture* src, const TextureView* srcView, const CopyRegion* srcRegion, const Texture* dst, const TextureView* dstView, const CopyRegion* dstRegion, int flags);

	static void (*nvnWindowGetCrop_0)(const Window* window, Rectangle* crop);
	static void (*nvnCommandBufferSetTexturePool_0)(const CommandBuffer* cmdBuf, const TexturePool* pool);
	static void (*nvnCommandBufferSetSamplerPool_0)(const CommandBuffer* cmdBuf, const SamplerPool* pool);
	static const TexturePool* volatile gameTexturePool = nullptr;
	static const SamplerPool* volatile gameSamplerPool = nullptr;

	constexpr size_t COMMAND_MEMORY_PER_BUF = 0x1000; 
	constexpr size_t CONTROL_MEMORY_PER_BUF = 0x1000;
	int COUNTER_ALIGNMENT = 0x10;

	struct nvnCounterData {
		uint64_t empty;
		uint64_t timestamp;
		uint64_t samplesPassed;
		uint64_t timestamp1;
		uint64_t inputVertices;
		uint64_t timestamp2;
		uint64_t inputPrimitives;
		uint64_t timestamp3;
		uint64_t vertexShaderInvocations;
		uint64_t timestamp4;
		uint64_t tessControlShaderInvocations;
		uint64_t timestamp5;
		uint64_t tessEvaluationShaderInvocations;
		uint64_t timestamp6;
		uint64_t geometryShaderInvocations;
		uint64_t timestamp7;
		uint64_t fragmentShaderInvocations;
		uint64_t timestamp8;
		uint64_t tessEvaluationShaderPrimitives;
		uint64_t timestamp9;
		uint64_t geometryShaderPrimitives;
		uint64_t timestamp10;
		uint64_t clipperInputPrimitives;
		uint64_t timestamp11;
		uint64_t clipperOutputPrimitives;
		uint64_t timestamp12;
		uint64_t primitivesGenerated;
		uint64_t timestamp13;
		uint64_t transformFeedbackPrimitivesWritten;
		uint64_t timestamp14;
		uint32_t tilesProcessedByZcull;
		uint32_t pixelBlocksBehindPrimitivesAndCulled;
		uint32_t pixelBlocksInFrontOfPrimitivesCulled;
		uint32_t pixelBlocksFailedStencilTestAndCulled;
	};

	MemoryPool timestampDataPool{};
	nvnCounterData* timestampDataCPU = 0;
	MemoryPool profilingCmdMemoryPool{};
	alignas(0x1000) char controlMemoryStorage[0x1000]{};
	Sync timestampSync{};
	CommandBuffer tsCmdBuf{};
	alignas(0x1000) char dataPoolHostPtr[0x1000]{};
	alignas(0x1000) char cmdPoolHostPtr[0x1000]{};
	CommandHandle cmdHandles{};
	bool enableCounters = false;

	namespace TripleBuffer {
		// We assume that there won't be bigger than 1920x1080 RGBA8 texture.
		// Per swizzling requirements height must be aligned to 1152.
		// Size aligned to 0x1000;
		constexpr int GAME_TEXTURES = 2;
		constexpr size_t COMMAND_MEMORY_SIZE = 0x1000;
		constexpr size_t CONTROL_MEMORY_SIZE = 0x1000;
		alignas(0x1000) static char copyCommandMemoryStorage[COMMAND_MEMORY_SIZE]{};
		alignas(0x1000) static char copyControlMemoryStorage[CONTROL_MEMORY_SIZE]{};
		
		constexpr int MEMORY_POOL_FLAGS_CPU_NO_ACCESS = 0x1;
		constexpr int MEMORY_POOL_FLAGS_CPU_UNCACHED = 0x2;
		constexpr int MEMORY_POOL_FLAGS_GPU_CACHED = 0x20;
		constexpr int MEMORY_POOL_FLAGS_COMPRESSIBLE = 0x80;
		constexpr int BARRIER_ORDER_FRAGMENTS = 0x2;
		constexpr int BARRIER_INVALIDATE_TEXTURE = 0x10;
		constexpr int SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE = 0;
		constexpr uint64_t WAIT_TIMEOUT_MAXIMUM = UINT64_MAX;

		bool requested = false;
		bool texturesInUse = false;
		bool poolInitialized = false;
		bool texturesInitialized = false;
		bool cmdBufInitialized = false;
		bool cmdPoolInitialized = false;

		uintptr_t memory = 0;
		size_t memorySize = 0;

		MemoryPool texturePool{};
		Texture textures[WINDOW_TEXTURES]{};
		const Texture* windowTextures[WINDOW_TEXTURES]{};
		const Texture* gameTextures[GAME_TEXTURES]{};
		int texWidth = 0, texHeight = 0, texFormat = 0, texFlags = 0, texTarget = 0;

		TextureBuilder textureBuilder{};
		MemoryPoolBuilder memoryPoolBuilder{};

		MemoryPool cmdPool{};
		CommandBuffer cmdBuf{};
		CommandHandle copyHandles[GAME_TEXTURES][WINDOW_TEXTURES]{};

		Sync frameSyncs[GAME_TEXTURES]{};
		bool frameSyncPending[GAME_TEXTURES]{};
		bool frameSyncsInitialized = false;

		const WindowBuilder* activeBuilder = nullptr;
		const Window* activeWindow = nullptr;
		int gameIndex = 0;
		int windowIndex = 0;
		bool acquired = false;

		// Prepares a texture builder for our window texture.
		void setupBuilder(TextureBuilder* builder) {
			nvnTextureBuilderSetDefaults_0(builder);
			nvnTextureBuilderSetDevice_0(builder, mainDevice);
			nvnTextureBuilderSetFlags_0(builder, texFlags);
			nvnTextureBuilderSetTarget_0(builder, texTarget);
			nvnTextureBuilderSetFormat_0(builder, texFormat);
			nvnTextureBuilderSetSize2D_0(builder, texWidth, texHeight);
		}

		bool createPool() {
			if (poolInitialized) return true;
			MemoryPoolBuilder* poolBuilder = &memoryPoolBuilder;
			nvnMemoryPoolBuilderSetDefaults_0(poolBuilder);
			nvnMemoryPoolBuilderSetDevice_0(poolBuilder, mainDevice);
			nvnMemoryPoolBuilderSetFlags_0(poolBuilder, MEMORY_POOL_FLAGS_CPU_NO_ACCESS | MEMORY_POOL_FLAGS_GPU_CACHED | MEMORY_POOL_FLAGS_COMPRESSIBLE);
			nvnMemoryPoolBuilderSetStorage_0(poolBuilder, (void*)memory, TEXTURE_MEMORY_SIZE);
			poolInitialized = nvnMemoryPoolInitialize_0(&texturePool, poolBuilder);
			if (!poolInitialized) requested = false;
			return poolInitialized;
		}

		bool createTextures(const Texture* reference) {
			const int width = nvnTextureGetWidth_0(reference);
			const int height = nvnTextureGetHeight_0(reference);
			const int format = nvnTextureGetFormat_0(reference);
			const int flags = nvnTextureGetFlags_0(reference);
			const int target = nvnTextureGetTarget_0(reference);

			if (texturesInitialized) {
				if (width == texWidth && height == texHeight && format == texFormat && flags == texFlags && target == texTarget)
					return true;
				// Game recreated its window with other textures (f.e. Alan Wake: 1280x720 in handheld, 1920x1080 in dock).
				// Ours can be recreated only when no window uses them anymore.
				if (texturesInUse || !nvnTextureFinalize_0) return false;
				for (int i = 0; i < WINDOW_TEXTURES; i++) nvnTextureFinalize_0(&textures[i]);
				texturesInitialized = false;
			}

			texWidth = width; texHeight = height; texFormat = format; texFlags = flags; texTarget = target;

			TextureBuilder* builder = &textureBuilder;
			setupBuilder(builder);
			const size_t storageSize = nvnTextureBuilderGetStorageSize_0(builder);
			size_t alignment = nvnTextureBuilderGetStorageAlignment_0(builder);
			if (!alignment) alignment = 0x1000;
			const size_t stride = (storageSize + alignment - 1) & ~(alignment - 1);
			if (!storageSize || stride * WINDOW_TEXTURES > TEXTURE_MEMORY_SIZE) {
				return false;
			}

			for (int i = 0; i < WINDOW_TEXTURES; i++) {
				setupBuilder(builder);
				nvnTextureBuilderSetStorage_0(builder, &texturePool, (ptrdiff_t)(stride * i));
				if (!nvnTextureInitialize_0(&textures[i], builder)) {
					requested = false;
					return false;
				}
				windowTextures[i] = &textures[i];
			}
			texturesInitialized = true;
			return true;
		}

		bool recordCopies() {
			if (cmdBufInitialized) {
				nvnCommandBufferFinalize_0(&cmdBuf);
				cmdBufInitialized = false;
			}
			if (!cmdPoolInitialized) {
				MemoryPoolBuilder* poolBuilder = &memoryPoolBuilder;
				nvnMemoryPoolBuilderSetDefaults_0(poolBuilder);
				nvnMemoryPoolBuilderSetDevice_0(poolBuilder, mainDevice);
				nvnMemoryPoolBuilderSetFlags_0(poolBuilder, MEMORY_POOL_FLAGS_CPU_UNCACHED | MEMORY_POOL_FLAGS_GPU_CACHED);
				nvnMemoryPoolBuilderSetStorage_0(poolBuilder, (void*)copyCommandMemoryStorage, COMMAND_MEMORY_SIZE);
				if (!nvnMemoryPoolInitialize_0(&cmdPool, poolBuilder)) {
					return false;
				}
				cmdPoolInitialized = true;
			}
			if (!nvnCommandBufferInitialize_0(&cmdBuf, mainDevice)) {
				return false;
			}
			cmdBufInitialized = true;
			nvnCommandBufferAddCommandMemory_0(&cmdBuf, &cmdPool, 0, COMMAND_MEMORY_SIZE);
			nvnCommandBufferAddControlMemory_0(&cmdBuf, (void*)copyControlMemoryStorage, CONTROL_MEMORY_SIZE);

			CopyRegion region{};
			region.x = 0;
			region.y = 0;
			region.z = 0;
			region.width = texWidth;
			region.height = texHeight;
			region.depth = 1;
			for (int src = 0; src < GAME_TEXTURES; src++) {
				for (int dst = 0; dst < WINDOW_TEXTURES; dst++) {
					nvnCommandBufferBeginRecording_0(&cmdBuf);
					nvnCommandBufferBarrier_0(&cmdBuf, BARRIER_ORDER_FRAGMENTS | BARRIER_INVALIDATE_TEXTURE);
					nvnCommandBufferCopyTextureToTexture_0(&cmdBuf, gameTextures[src], nullptr, &region, &textures[dst], nullptr, &region, 0);
					copyHandles[src][dst] = nvnCommandBufferEndRecording_0(&cmdBuf);
				}
			}
			return true;
		}

		// Returns textures that should be passed to window or nullptr if emulation can't be used.
		const Texture** setup(const WindowBuilder* builder, int numTextures, const Texture** textures) {
			if (!requested || numTextures != GAME_TEXTURES || !textures || !textures[0] || !textures[1]) return nullptr;
			if (!memory) {
				memory = SaltySDCore_GetReservedMemory(&memorySize);
				if (!memory || memorySize < RESERVED_MEMORY_SIZE) {
					requested = false;
					return nullptr;
				}
			}
			if (!frameSyncsInitialized) {
				for (int i = 0; i < GAME_TEXTURES; i++) {
					if (!nvnSyncInitialize_0(&frameSyncs[i], mainDevice)) {
						requested = false;
						return nullptr;
					}
				}
				frameSyncsInitialized = true;
			}
			if (!createPool()) return nullptr;
			// Wait until GPU is done with our copies before touching textures or command memory.
			for (int i = 0; i < GAME_TEXTURES; i++) {
				if (frameSyncPending[i]) {
					nvnSyncWait_0(&frameSyncs[i], WAIT_TIMEOUT_MAXIMUM);
					frameSyncPending[i] = false;
				}
			}
			if (!createTextures(textures[0])) return nullptr;
			// Copies are always recorded again. A copy command stores the source texture's GPU storage
			// at record time, not the Texture pointer, and games can recreate their textures at the
			// same addresses (Alan Wake frees its 2 backbuffers on every dock/handheld switch and
			// allocates new ones that land in the same place). Comparing pointers then kept the old
			// copies, which kept showing the last frame from the freed textures.
			gameTextures[0] = textures[0];
			gameTextures[1] = textures[1];
			if (!recordCopies()) return nullptr;
			activeBuilder = builder;
			activeWindow = nullptr;
			gameIndex = 0;
			acquired = false;
			frameSyncPending[0] = false;
			frameSyncPending[1] = false;
			return windowTextures;
		}
	}

	constexpr int MAX_PRESENTED_TEXTURES = 4;
	const Texture* presentedTextures[MAX_PRESENTED_TEXTURES]{};
	int presentedTextureCount = 0;

	namespace Logo {
		bool done = false;
		uint64_t startTick = 0;
		uint64_t endTick = 0;

		void* Resolve(const char* name) {
			return (void*)nvnDeviceGetProcAddress_0(mainDevice, name);
		}

		bool offsetScanAttempted = false;
		bool offsetScanSucceeded = false;
		uint32_t foundTexturesOffset = 0;

#if !defined(SWITCH32) && !defined(OUNCE32)
		uint32_t matchStrXImm(uint32_t word, int rt, int rn) {
			if ((word & 0xFFC00000u) != 0xF9000000u) return 0xFFFFFFFFu;
			if ((int)(word & 0x1F) != rt) return 0xFFFFFFFFu;
			if ((int)((word >> 5) & 0x1F) != rn) return 0xFFFFFFFFu;
			return ((word >> 10) & 0xFFF) * 8;
		}

		void FindWindowBuilderOffsets() {
			offsetScanAttempted = true;

			uintptr_t addr = (uintptr_t)nvnWindowBuilderSetTextures_0;
			if (!addr) return;

			const uint32_t* code = (const uint32_t*)addr;
			for (int i = 0; i < 64; i++) {
				uint32_t word = code[i];
				// Stop scanning once we hit RET (0xD65F03C0)
				if (word == 0xD65F03C0u) break;

				uint32_t off = matchStrXImm(word, /*Xt=*/2, /*Xn=*/0);
				if (off != 0xFFFFFFFFu) {
					foundTexturesOffset = off;
					offsetScanSucceeded = true;
					return;
				}
			}
		}
#else
		uint32_t matchStrImmA32(uint32_t word, int rt, int rn) {
			if ((word & 0x0F700000u) != 0x05000000u) return 0xFFFFFFFFu;
			if ((int)((word >> 16) & 0xF) != rn) return 0xFFFFFFFFu;
			if ((int)((word >> 12) & 0xF) != rt) return 0xFFFFFFFFu;
			return word & 0xFFF;
		}

		void FindWindowBuilderOffsets() {
			offsetScanAttempted = true;

			uintptr_t addr = (uintptr_t)nvnWindowBuilderSetTextures_0;
			if (!addr) return;
			
			const uint32_t* code = (const uint32_t*)addr;
			for (int i = 0; i < 64; i++) {
				uint32_t word = code[i];
				// Stop scanning once we hit BX LR (0xE12FFF1E, unconditional)
				if ((word & 0x0FFFFFFFu) == 0x012FFF1Eu) break;

				uint32_t off = matchStrImmA32(word, /*Rt=*/2, /*Rn=*/0);
				if (off != 0xFFFFFFFFu) {
					foundTexturesOffset = off;
					offsetScanSucceeded = true;
					return;
				}
			}
		}
#endif

		bool RecoverFromBuilder(const WindowBuilder* windowBuilder) {
			if (!offsetScanAttempted) FindWindowBuilderOffsets();
			if (!offsetScanSucceeded) return false;

			int numBufferedFrames = windowBuilder->numBufferedFrames;
			const uint8_t* base = (const uint8_t*)windowBuilder;
			const Texture** texturesPtr = *(const Texture***)(base + foundTexturesOffset);
			if (numBufferedFrames <= 0 || numBufferedFrames > MAX_PRESENTED_TEXTURES || !texturesPtr) return false;

			presentedTextureCount = numBufferedFrames;
			for (int i = 0; i < presentedTextureCount; i++) presentedTextures[i] = texturesPtr[i];
			return true;
		}

		// Right before present: target holds the finished frame.
		void Draw(const Queue* queue, const Texture* target, const Rectangle& crop) {
			if (!mainDevice || !target) return;
			const uint64_t now = Utils::_getSystemTick();
			if (endTick == 0) {
				startTick = now;
				endTick = now + systemtickfrequency * ::Logo::DURATION_SECONDS;
			}
			if (now >= endTick) {
				done = true;
				return;
			}
			const float time = (float)((double)(now - startTick) / (double)systemtickfrequency);
			if (!LogoNVN::Draw(mainDevice, queue, target, nvnTextureGetWidth_0(target), nvnTextureGetHeight_0(target), crop, time,
			                   gameTexturePool, gameSamplerPool, Resolve))
				done = true;
		}
	}

	void WindowBuilderSetTextures(const WindowBuilder* nvnWindowBuilder, int numBufferedFrames, const Texture** nvnTextures) {
		if (const Texture** emulated = TripleBuffer::setup(nvnWindowBuilder, numBufferedFrames, nvnTextures)) {
			(Shared -> Buffers) = TripleBuffer::WINDOW_TEXTURES;
			(Shared -> ActiveBuffers) = TripleBuffer::WINDOW_TEXTURES;
			amountOfAvailableBuffers = TripleBuffer::WINDOW_TEXTURES;
			presentedTextureCount = (TripleBuffer::WINDOW_TEXTURES < MAX_PRESENTED_TEXTURES) ? TripleBuffer::WINDOW_TEXTURES : MAX_PRESENTED_TEXTURES;
			for (int i = 0; i < presentedTextureCount; i++) presentedTextures[i] = emulated[i];
			return nvnWindowBuilderSetTextures_0(nvnWindowBuilder, TripleBuffer::WINDOW_TEXTURES, emulated);
		}
		if (TripleBuffer::activeBuilder == nvnWindowBuilder) TripleBuffer::activeBuilder = nullptr;
		presentedTextureCount = (numBufferedFrames < MAX_PRESENTED_TEXTURES) ? numBufferedFrames : MAX_PRESENTED_TEXTURES;
		for (int i = 0; i < presentedTextureCount; i++) presentedTextures[i] = nvnTextures[i];
		(Shared -> Buffers) = numBufferedFrames;
		amountOfAvailableBuffers = numBufferedFrames;
		if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= numBufferedFrames) {
			if (!setNumActiveTexturesDetected) numBufferedFrames = (Shared -> SetBuffers);
			else Shared->expectedSetBuffers = (Shared -> SetBuffers);
		}
		(Shared -> ActiveBuffers) = numBufferedFrames;
		return nvnWindowBuilderSetTextures_0(nvnWindowBuilder, numBufferedFrames, nvnTextures);
	}

	bool WindowInitialize(const Window* nvnWindow, WindowBuilder* windowBuilder) {
		// SetTextures hook was skipped (game filled the builder directly): textures come from the builder.
		if (presentedTextureCount == 0 && (!Logo::done || TripleBuffer::requested) && Logo::RecoverFromBuilder(windowBuilder) && TripleBuffer::requested) {
			// Copy, because WindowBuilderSetTextures replaces presentedTextures with our textures.
			static const Texture* textures[MAX_PRESENTED_TEXTURES]{};
			for (int i = 0; i < presentedTextureCount; i++) textures[i] = presentedTextures[i];
			WindowBuilderSetTextures(windowBuilder, presentedTextureCount, textures);
		}
		if (TripleBuffer::activeBuilder && windowBuilder == TripleBuffer::activeBuilder) {
			// All 3 textures must be active, whatever the game set on the builder.
			if (nvnWindowBuilderSetNumActiveTextures_0) nvnWindowBuilderSetNumActiveTextures_0(windowBuilder, TripleBuffer::WINDOW_TEXTURES);
			bool ret = nvnWindowInitialize_0(nvnWindow, windowBuilder);
			if (ret) {
				TripleBuffer::activeWindow = nvnWindow;
				TripleBuffer::texturesInUse = true;
				(Shared -> Buffers) = TripleBuffer::WINDOW_TEXTURES;
				(Shared -> ActiveBuffers) = TripleBuffer::WINDOW_TEXTURES;
			}
			return ret;
		}
		// Window initialized without our textures (f.e. object reused after finalize), don't emulate it.
		if (nvnWindow == TripleBuffer::activeWindow) TripleBuffer::activeWindow = nullptr;
		if (Shared->Buffers == 0) {
			(Shared -> Buffers) = windowBuilder -> numBufferedFrames;
			if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= windowBuilder -> numBufferedFrames) {
				windowBuilder -> numBufferedFrames = (Shared -> SetBuffers);
			}
			(Shared -> ActiveBuffers) = windowBuilder -> numBufferedFrames;	
		}
		return nvnWindowInitialize_0(nvnWindow, windowBuilder);
	}

	void WindowFinalize(const Window* nvnWindow) {
		if (nvnWindow && nvnWindow == TripleBuffer::activeWindow) {
			// Our copies may still be running on GPU.
			for (int i = 0; i < TripleBuffer::GAME_TEXTURES; i++) {
				if (TripleBuffer::frameSyncPending[i]) {
					nvnSyncWait_0(&TripleBuffer::frameSyncs[i], TripleBuffer::WAIT_TIMEOUT_MAXIMUM);
					TripleBuffer::frameSyncPending[i] = false;
				}
			}
			TripleBuffer::activeWindow = nullptr;
			TripleBuffer::acquired = false;
			TripleBuffer::texturesInUse = false;
		}
		nvnWindowFinalize_0(nvnWindow);
	}

	void WindowBuilderSetNumActiveTextures(const WindowBuilder* builder, int numActiveTextures) {
		if (TripleBuffer::activeBuilder && builder == TripleBuffer::activeBuilder) {
			numActiveTextures = TripleBuffer::WINDOW_TEXTURES;
		}
		else if (!setNumActiveTexturesDetected && (Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= (Shared -> Buffers)) {
			numActiveTextures = (Shared -> SetBuffers);
		}
		(Shared -> ActiveBuffers) = numActiveTextures;
		nvnWindowBuilderSetNumActiveTextures_0(builder, numActiveTextures);
	}

	void WindowSetNumActiveTextures(const Window* nvnWindow, int numBufferedFrames) {
		if (TripleBuffer::activeWindow && nvnWindow == TripleBuffer::activeWindow) {
			if (numBufferedFrames > 0) (Shared -> SetActiveBuffers) = numBufferedFrames;
			(Shared -> ActiveBuffers) = TripleBuffer::WINDOW_TEXTURES;
			return nvnWindowSetNumActiveTextures_0(nvnWindow, TripleBuffer::WINDOW_TEXTURES);
		}
		if (numBufferedFrames < 0) {
			numBufferedFrames *= -1;
			nvnWindowSetNumActiveTextures_0(nvnWindow, numBufferedFrames);
			(Shared -> ActiveBuffers) = nvnWindowGetNumActiveTextures_0(nvnWindow);
		}
		else {
			(Shared -> SetActiveBuffers) = numBufferedFrames;
			if (Shared->expectedSetBuffers > 0) return;
			if ((Shared -> SetBuffers) >= 2 && (Shared -> SetBuffers) <= (Shared -> Buffers)) {
				numBufferedFrames = (Shared -> SetBuffers);
			}
			(Shared -> ActiveBuffers) = numBufferedFrames;
		}
		return nvnWindowSetNumActiveTextures_0(nvnWindow, numBufferedFrames);
	}

	void SetPresentInterval(const Window* nvnWindow, int mode) {
		if (mode < 0) {
			mode *= -1;
			if ((Shared -> FPSmode) != mode) {
				nvnWindowSetPresentInterval_0(nvnWindow, mode);
				(Shared -> FPSmode) = mode;
			}
			changedFPS = true;
		}
		else if (!changeFPS) {
			nvnWindowSetPresentInterval_0(nvnWindow, mode);
			changedFPS = false;
			(Shared -> FPSmode) = mode;
		}
		return;
	}

	void CommandBufferSetTexturePool(const CommandBuffer* cmdBuf, const TexturePool* pool) {
		gameTexturePool = pool;
		nvnCommandBufferSetTexturePool_0(cmdBuf, pool);
	}
	void CommandBufferSetSamplerPool(const CommandBuffer* cmdBuf, const SamplerPool* pool) {
		gameSamplerPool = pool;
		nvnCommandBufferSetSamplerPool_0(cmdBuf, pool);
	}
	int SyncWait0(const Sync* _this, uint64_t timeout_ns) {
		if (_this == WindowSync && (Shared -> ActiveBuffers) == 2) {
			if ((Shared -> ZeroSync) == ZeroSyncType_Semi) {
				const uint64_t endFrameTick = Utils::_getSystemTick();
				u64 FrameTarget = (systemtickfrequency/60) - 8000;
				s64 new_timeout = (FrameTarget - (endFrameTick - startFrameTick)) - (systemtickfrequency / 1000);
				if (((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked)) == 60) {
					new_timeout = (systemtickfrequency/101) - (endFrameTick - startFrameTick);
				}
				if (new_timeout > 0) {
					timeout_ns = Utils::_convertToTimeSpan(new_timeout);
				}
				else timeout_ns = 0;
			}
			else if ((Shared -> ZeroSync) == ZeroSyncType_Soft) 
				timeout_ns = 0;
		}
		return nvnSyncWait_0(_this, timeout_ns);
	}

	bool DeviceInitialize(Device* nvnDevice, const DeviceBuilder* nvnDeviceBuilder) {
		bool ret = nvnDeviceInitialize_0(nvnDevice, nvnDeviceBuilder);
		if (mainDevice == 0) mainDevice = nvnDevice;
		return ret;
	}

	void PresentTexture(const Queue* queue, const Window* nvnWindow, int index) {

		bool m_enableCounters = enableCounters;
		if (NX_FPS_Math::starttick == 0) [[unlikely]] {
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;

			if (m_enableCounters) {
				MemoryPoolBuilder dataPoolBuilder{};
				nvnMemoryPoolBuilderSetDefaults_0(&dataPoolBuilder);
				nvnMemoryPoolBuilderSetDevice_0(&dataPoolBuilder, mainDevice);
				nvnMemoryPoolBuilderSetFlags_0(&dataPoolBuilder, BIT(1) | BIT(5));
				nvnMemoryPoolBuilderSetStorage_0(&dataPoolBuilder, (void*)&dataPoolHostPtr, sizeof(dataPoolHostPtr));
				nvnMemoryPoolInitialize_0(&timestampDataPool, &dataPoolBuilder);
				timestampDataCPU = (nvnCounterData*)nvnMemoryPoolMap_0(&timestampDataPool);
				BufferAddress dataGpuAddress = nvnMemoryPoolGetBufferAddress_0(&timestampDataPool);

				MemoryPoolBuilder cmdPoolBuilder{};
				nvnMemoryPoolBuilderSetDefaults_0(&cmdPoolBuilder);
				nvnMemoryPoolBuilderSetDevice_0(&cmdPoolBuilder, mainDevice);
				nvnMemoryPoolBuilderSetFlags_0(&cmdPoolBuilder, BIT(1) | BIT(5));
				nvnMemoryPoolBuilderSetStorage_0(&cmdPoolBuilder, (void*)&cmdPoolHostPtr, sizeof(cmdPoolHostPtr));
				nvnMemoryPoolInitialize_0(&profilingCmdMemoryPool, &cmdPoolBuilder);

				//nvnDeviceGetInteger_0(mainDevice, 10, &COUNTER_ALIGNMENT);

				nvnCommandBufferInitialize_0(&tsCmdBuf, mainDevice);
				
				nvnCommandBufferAddCommandMemory_0(&tsCmdBuf, &profilingCmdMemoryPool, 0, COMMAND_MEMORY_PER_BUF);
				nvnCommandBufferAddControlMemory_0(&tsCmdBuf, controlMemoryStorage, CONTROL_MEMORY_PER_BUF);

				nvnCommandBufferBeginRecording_0(&tsCmdBuf);

				for (size_t counter = 0; counter < 0x10; counter++) {
					BufferAddress currentFrameOffset = dataGpuAddress + (counter * COUNTER_ALIGNMENT);
					nvnCommandBufferReportCounter_0(&tsCmdBuf, counter, currentFrameOffset);
					nvnCommandBufferResetCounter_0(&tsCmdBuf, counter);			
				}
				
				cmdHandles = nvnCommandBufferEndRecording_0(&tsCmdBuf);
			}
		}
		NX_FPS_Math::PreFrame();

		Rectangle crop{};
		nvnWindowGetCrop_0(nvnWindow, &crop);

		if (TripleBuffer::activeWindow && nvnWindow == TripleBuffer::activeWindow && TripleBuffer::acquired && (index == 0 || index == 1)) {
			nvnQueueSubmitCommands_0(queue, 1, &TripleBuffer::copyHandles[index][TripleBuffer::windowIndex]);

			if (!Logo::done) Logo::Draw(queue, TripleBuffer::windowTextures[TripleBuffer::windowIndex], crop);

			nvnQueueFenceSync_0(queue, &TripleBuffer::frameSyncs[index], TripleBuffer::SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE, 0);
			TripleBuffer::frameSyncPending[index] = true;
			nvnQueuePresentTexture_0(queue, nvnWindow, TripleBuffer::windowIndex);
			TripleBuffer::gameIndex ^= 1;
			TripleBuffer::acquired = false;
		}
		else {
			if (!Logo::done && index >= 0 && index < presentedTextureCount) Logo::Draw(queue, presentedTextures[index], crop);
			nvnQueuePresentTexture_0(queue, nvnWindow, index);
		}
		if (m_enableCounters) {
			Shared->PerfCounters.NVN.timestamp = timestampDataCPU->timestamp;
			#if defined(SWITCH) || defined(OUNCE)
			uint64x2x4_t loaded_data1 = vld1q_u64_x4(&timestampDataCPU->samplesPassed);
			uint64x2x4_t loaded_data2 = vld1q_u64_x4(&timestampDataCPU->tessControlShaderInvocations);
			uint64x2x4_t loaded_data3 = vld1q_u64_x4(&timestampDataCPU->tessEvaluationShaderPrimitives);
			uint64x2x2_t loaded_data4 = vld1q_u64_x2(&timestampDataCPU->primitivesGenerated);
			uint32x4_t loaded_data5 = vld1q_u32(&timestampDataCPU->tilesProcessedByZcull);

			Shared->PerfCounters.NVN.samplesPassed = loaded_data1.val[0][0];
			Shared->PerfCounters.NVN.inputVertices = loaded_data1.val[1][0];
			Shared->PerfCounters.NVN.inputPrimitives = loaded_data1.val[2][0];
			Shared->PerfCounters.NVN.vertexShaderInvocations = loaded_data1.val[3][0];

			Shared->PerfCounters.NVN.tessControlShaderInvocations = loaded_data2.val[0][0];
			Shared->PerfCounters.NVN.tessEvaluationShaderInvocations = loaded_data2.val[1][0];
			Shared->PerfCounters.NVN.geometryShaderInvocations = loaded_data2.val[2][0];
			Shared->PerfCounters.NVN.fragmentShaderInvocations = loaded_data2.val[3][0];

			Shared->PerfCounters.NVN.tessEvaluationShaderPrimitives = loaded_data3.val[0][0];
			Shared->PerfCounters.NVN.geometryShaderPrimitives = loaded_data3.val[1][0];
			Shared->PerfCounters.NVN.clipperInputPrimitives = loaded_data3.val[2][0];
			Shared->PerfCounters.NVN.clipperOutputPrimitives = loaded_data3.val[3][0];

			Shared->PerfCounters.NVN.primitivesGenerated = loaded_data4.val[0][0];
			Shared->PerfCounters.NVN.transformFeedbackPrimitivesWritten = loaded_data4.val[1][0];

			Shared->PerfCounters.NVN.tilesProcessedByZcull = loaded_data5[0];
			Shared->PerfCounters.NVN.pixelBlocksBehindPrimitivesAndCulled = loaded_data5[1];
			Shared->PerfCounters.NVN.pixelBlocksInFrontOfPrimitivesCulled = loaded_data5[2];
			Shared->PerfCounters.NVN.pixelBlocksFailedStencilTestAndCulled = loaded_data5[3];
			#else
			Shared->PerfCounters.NVN.samplesPassed = timestampDataCPU->samplesPassed;
			Shared->PerfCounters.NVN.inputVertices = timestampDataCPU->inputVertices;
			Shared->PerfCounters.NVN.inputPrimitives = timestampDataCPU->inputPrimitives;
			Shared->PerfCounters.NVN.vertexShaderInvocations = timestampDataCPU->vertexShaderInvocations;
			Shared->PerfCounters.NVN.tessControlShaderInvocations = timestampDataCPU->tessControlShaderInvocations;
			Shared->PerfCounters.NVN.tessEvaluationShaderInvocations = timestampDataCPU->tessEvaluationShaderInvocations;
			Shared->PerfCounters.NVN.geometryShaderInvocations = timestampDataCPU->geometryShaderInvocations;
			Shared->PerfCounters.NVN.fragmentShaderInvocations = timestampDataCPU->fragmentShaderInvocations;
			Shared->PerfCounters.NVN.tessEvaluationShaderPrimitives = timestampDataCPU->tessEvaluationShaderPrimitives;
			Shared->PerfCounters.NVN.geometryShaderPrimitives = timestampDataCPU->geometryShaderPrimitives;
			Shared->PerfCounters.NVN.clipperInputPrimitives = timestampDataCPU->clipperInputPrimitives;
			Shared->PerfCounters.NVN.clipperOutputPrimitives = timestampDataCPU->clipperOutputPrimitives;
			Shared->PerfCounters.NVN.primitivesGenerated = timestampDataCPU->primitivesGenerated;
			Shared->PerfCounters.NVN.transformFeedbackPrimitivesWritten = timestampDataCPU->transformFeedbackPrimitivesWritten;
			Shared->PerfCounters.NVN.tilesProcessedByZcull = timestampDataCPU->tilesProcessedByZcull;
			Shared->PerfCounters.NVN.pixelBlocksBehindPrimitivesAndCulled = timestampDataCPU->pixelBlocksBehindPrimitivesAndCulled;
			Shared->PerfCounters.NVN.pixelBlocksInFrontOfPrimitivesCulled = timestampDataCPU->pixelBlocksInFrontOfPrimitivesCulled;
			Shared->PerfCounters.NVN.pixelBlocksFailedStencilTestAndCulled = timestampDataCPU->pixelBlocksFailedStencilTestAndCulled;
			#endif
			nvnQueueSubmitCommands_0(queue, 1, &cmdHandles);
		}
		NX_FPS_Math::PostFrame();

		if (setNumActiveTexturesDetected && nvnWindow != TripleBuffer::activeWindow) {
			auto expectedBuffers = Shared->expectedSetBuffers;
			if ((expectedBuffers > 0) && (amountOfAvailableBuffers >= expectedBuffers) && (expectedBuffers != (Shared->ActiveBuffers))) {
				expectedBuffers *= -1;
				nvnQueueFinish_0(queue);
				WindowSetNumActiveTextures(nvnWindow, expectedBuffers);
			}
		}

		const auto nvnInterval = nvnWindowGetPresentInterval_0(nvnWindow);

		(Shared -> FPSmode) = (uint8_t)nvnInterval;

		const auto new_fpslock = NX_FPS_Math::new_fpslock;
		
		if (!new_fpslock) {
			NX_FPS_Math::FPStiming = 0;
			NX_FPS_Math::FPSlock = 0;
			changeFPS = false;
		}
		else {
			changeFPS = true;
			const auto currentRefreshRate = (Shared -> currentRefreshRate);
			NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
			const uint32_t rr = currentRefreshRate ? currentRefreshRate : 60;
			uint32_t threshold;
			int interval;
			if      (new_fpslock <= rr / 4) {interval = 4; threshold = rr / 4;}
			else if (new_fpslock <= rr / 3) {interval = 3; threshold = rr / 3;}
			else if (new_fpslock <= rr / 2) {interval = 2; threshold = rr / 2;}
			else                            {interval = 1; threshold = rr;    }

			if (nvnInterval != interval) {
				if (interval == 1)
					NVN::SetPresentInterval(nvnWindow, -2); //This allows in game with glitched interval to unlock 60 FPS, f.e. WRC Generations
				NVN::SetPresentInterval(nvnWindow, interval * -1);
			}

			if (new_fpslock != threshold || Shared->ZeroSync) {
				NX_FPS_Math::FPStiming = (systemtickfrequency / new_fpslock) - 6000;
				if (new_fpslock == threshold)
					NX_FPS_Math::FPStiming -= 2000;
			} 
			else NX_FPS_Math::FPStiming = 0;
		}
		
		return;
	}

	Result AcquireTexture(const Window* nvnWindow, const Sync* nvnSync, const int* index) {
		if (WindowSync != nvnSync) {
			WindowSync = (Sync*)nvnSync;
		}
		Result ret = nvnWindowAcquireTexture_0(nvnWindow, nvnSync, index);
		if (R_SUCCEEDED(ret) && TripleBuffer::activeWindow && nvnWindow == TripleBuffer::activeWindow && index) {
			// Game gets only its own 2 textures, alternating with each present. Real window texture is used as copy target.
			const int realIndex = *index;
			TripleBuffer::acquired = (ret == 0 && realIndex >= 0 && realIndex < TripleBuffer::WINDOW_TEXTURES);
			if (TripleBuffer::acquired) {
				TripleBuffer::windowIndex = realIndex;
			}
			const int gameIndex = TripleBuffer::gameIndex;
			if (TripleBuffer::frameSyncPending[gameIndex]) {
				nvnSyncWait_0(&TripleBuffer::frameSyncs[gameIndex], TripleBuffer::WAIT_TIMEOUT_MAXIMUM);
				TripleBuffer::frameSyncPending[gameIndex] = false;
			}
			*(int*)index = gameIndex;
		}
		
		startFrameTick = Utils::_getSystemTick();
		return ret;
	}

	int QueueAcquireTexture(const Queue* queue, const Window* nvnWindow, int* index) {
		const int ret = nvnQueueAcquireTexture_0(queue, nvnWindow, index);
		if (ret == 0 && TripleBuffer::activeWindow && nvnWindow == TripleBuffer::activeWindow && index) {
			const int realIndex = *index;
			TripleBuffer::acquired = (realIndex >= 0 && realIndex < TripleBuffer::WINDOW_TEXTURES);
			if (TripleBuffer::acquired) {
				TripleBuffer::windowIndex = realIndex;
			}
			const int gameIndex = TripleBuffer::gameIndex;
			if (TripleBuffer::frameSyncPending[gameIndex]) {
				nvnSyncWait_0(&TripleBuffer::frameSyncs[gameIndex], TripleBuffer::WAIT_TIMEOUT_MAXIMUM);
				TripleBuffer::frameSyncPending[gameIndex] = false;
			}
			*index = gameIndex;
		}
		return ret;
	}


	void* CommandBufferSetViewports(CommandBuffer* cmdBuf, int start, int count, const Viewport* viewports) {
		if (resolutionLookup) for (int i = start; i < start+count; i++) {
			if (viewports[i].height > 1.f && viewports[i].width > 1.f && viewports[i].x == 0.f && viewports[i].y == 0.f) {
				uint32_t width = (uint32_t)viewports[i].width;
				uint32_t height = (uint32_t)viewports[i].height;
				NX_FPS_Math::addResToViewports(width, height);
				last_viewport = {width, height};
			}
		}
		return nvnCommandBufferSetViewports_0(cmdBuf, start, count, viewports);
	}

	void* CommandBufferSetViewport(CommandBuffer* cmdBuf, int x, int y, int width, int height) {
		if (!x && !y && height > 1 && width > 1 && resolutionLookup) {
			NX_FPS_Math::addResToViewports((uint32_t)width, (uint32_t)height);
				last_viewport = {(uint32_t)width, (uint32_t)height};
		}
		return nvnCommandBufferSetViewport_0(cmdBuf, x, y, width, height);
	}

	void* CommandBufferSetScissors(CommandBuffer* cmdBuf, int start, int count, const Scissor* viewports) {
		if (resolutionLookup) for (int i = start; i < start+count; i++) {
			if (viewports[i].height > 1 && viewports[i].width > 1 && viewports[i].x == 0 && viewports[i].y == 0 && (uint32_t)viewports[i].height != last_viewport.second && (uint32_t)viewports[i].width != last_viewport.first) {
				NX_FPS_Math::addResToViewports((uint32_t)viewports[i].width, (uint32_t)viewports[i].height);
			}
		}
		return nvnCommandBufferSetScissors_0(cmdBuf, start, count, viewports);
	}

	void* CommandBufferSetScissor(CommandBuffer* cmdBuf, int x, int y, int width, int height) {
		if (!x && !y && height > 1 && width > 1 && resolutionLookup && (uint32_t)width != last_viewport.first && (uint32_t)height != last_viewport.second) {
			NX_FPS_Math::addResToViewports((uint32_t)width, (uint32_t)height);
		}
		return nvnCommandBufferSetScissor_0(cmdBuf, x, y, width, height);
	}

	void* CommandBufferSetRenderTargets(CommandBuffer* cmdBuf, int numTextures, const Texture** texture, const TextureView** textureView, const Texture* depthTexture, const TextureView* depthView) {
		if (texture != NULL && depthTexture != NULL && resolutionLookup) {
			auto depth_width = nvnTextureGetWidth_0(depthTexture);
			auto depth_height = nvnTextureGetHeight_0(depthTexture);
			auto depth_format = nvnTextureGetFormat_0(depthTexture);
			if (depth_width > 1 && depth_height > 1 && (depth_format >= 51 && depth_format <= 54)) {
				NX_FPS_Math::addResToRender((uint32_t)depth_width, (uint32_t)depth_height);
			}
		}
		return nvnCommandBufferSetRenderTargets_0(cmdBuf, numTextures, texture, textureView, depthTexture, depthView);
	}

	void initWindowSetNumActiveTextures(bool* out) {
		setNumActiveTexturesDetected = true;
		Shared->expectedSetBuffers = 0;
		*out = true;
	}

	uintptr_t GetProcAddress0 (Device* nvnDevice, const char* nvnFunction);

	uintptr_t GetProcAddress0 (Device* nvnDevice, const char* nvnFunction) {
		if (nvnDevice) mainDevice = nvnDevice;
		uintptr_t address = nvnDeviceGetProcAddress_0(nvnDevice, nvnFunction);

		std::array nvn_replacements = {
			runtime_replace{"nvnDeviceGetProcAddress", nullptr, (void*)GetProcAddress0},
			runtime_replace{"nvnDeviceInitialize", (uintptr_t*)&nvnDeviceInitialize_0, (void*)DeviceInitialize},
			runtime_replace{"nvnQueuePresentTexture", (uintptr_t*)&nvnQueuePresentTexture_0, (void*)PresentTexture},
			runtime_replace{"nvnWindowAcquireTexture", (uintptr_t*)&nvnWindowAcquireTexture_0, (void*)AcquireTexture},
			runtime_replace{"nvnQueueAcquireTexture", (uintptr_t*)&nvnQueueAcquireTexture_0, (void*)QueueAcquireTexture},
			runtime_replace{"nvnWindowSetPresentInterval", (uintptr_t*)&nvnWindowSetPresentInterval_0, (void*)SetPresentInterval},
			runtime_replace{"nvnWindowGetPresentInterval", (uintptr_t*)&nvnWindowGetPresentInterval_0},
			runtime_replace{"nvnWindowSetNumActiveTextures", (uintptr_t*)&nvnWindowSetNumActiveTextures_0, (void*)WindowSetNumActiveTextures, initWindowSetNumActiveTextures},
			runtime_replace{"nvnWindowBuilderSetTextures", (uintptr_t*)&nvnWindowBuilderSetTextures_0, (void*)WindowBuilderSetTextures},
			runtime_replace{"nvnWindowInitialize", (uintptr_t*)&nvnWindowInitialize_0, (void*)WindowInitialize},
			runtime_replace{"nvnWindowFinalize", (uintptr_t*)&nvnWindowFinalize_0, (void*)WindowFinalize},
			runtime_replace{"nvnWindowBuilderSetNumActiveTextures", (uintptr_t*)&nvnWindowBuilderSetNumActiveTextures_0, (void*)WindowBuilderSetNumActiveTextures},
			runtime_replace{"nvnTextureFinalize", (uintptr_t*)&nvnTextureFinalize_0},
			runtime_replace{"nvnSyncWait", (uintptr_t*)&nvnSyncWait_0, (void*)SyncWait0},
			runtime_replace{"nvnCommandBufferSetRenderTargets", (uintptr_t*)&nvnCommandBufferSetRenderTargets_0, (void*)CommandBufferSetRenderTargets},
			runtime_replace{"nvnCommandBufferSetViewport", (uintptr_t*)&nvnCommandBufferSetViewport_0, (void*)CommandBufferSetViewport},
			runtime_replace{"nvnCommandBufferSetViewports", (uintptr_t*)&nvnCommandBufferSetViewports_0, (void*)CommandBufferSetViewports},
			runtime_replace{"nvnCommandBufferSetScissor", (uintptr_t*)&nvnCommandBufferSetScissor_0, (void*)CommandBufferSetScissor},
			runtime_replace{"nvnCommandBufferSetScissors", (uintptr_t*)&nvnCommandBufferSetScissors_0, (void*)CommandBufferSetScissors},
			runtime_replace{"nvnTextureGetWidth", (uintptr_t*)&nvnTextureGetWidth_0},
			runtime_replace{"nvnTextureGetHeight", (uintptr_t*)&nvnTextureGetHeight_0},
			runtime_replace{"nvnTextureGetFormat", (uintptr_t*)&nvnTextureGetFormat_0},
			runtime_replace{"nvnQueueFinish", (uintptr_t*)&nvnQueueFinish_0},
			runtime_replace{"nvnWindowGetNumActiveTextures", (uintptr_t*)&nvnWindowGetNumActiveTextures_0},
			runtime_replace{"nvnMemoryPoolBuilderSetDefaults", (uintptr_t*)&nvnMemoryPoolBuilderSetDefaults_0},
			runtime_replace{"nvnMemoryPoolBuilderSetDevice", (uintptr_t*)&nvnMemoryPoolBuilderSetDevice_0},
			runtime_replace{"nvnMemoryPoolBuilderSetFlags", (uintptr_t*)&nvnMemoryPoolBuilderSetFlags_0},
			runtime_replace{"nvnMemoryPoolBuilderSetStorage", (uintptr_t*)&nvnMemoryPoolBuilderSetStorage_0},
			runtime_replace{"nvnMemoryPoolInitialize", (uintptr_t*)&nvnMemoryPoolInitialize_0},
			runtime_replace{"nvnMemoryPoolMap", (uintptr_t*)&nvnMemoryPoolMap_0},
			runtime_replace{"nvnMemoryPoolGetBufferAddress", (uintptr_t*)&nvnMemoryPoolGetBufferAddress_0},
			runtime_replace{"nvnSyncInitialize", (uintptr_t*)&nvnSyncInitialize_0},
			runtime_replace{"nvnCommandBufferInitialize", (uintptr_t*)&nvnCommandBufferInitialize_0},
			runtime_replace{"nvnCommandBufferAddCommandMemory", (uintptr_t*)&nvnCommandBufferAddCommandMemory_0},
			runtime_replace{"nvnCommandBufferAddControlMemory", (uintptr_t*)&nvnCommandBufferAddControlMemory_0},
			runtime_replace{"nvnCommandBufferBeginRecording", (uintptr_t*)&nvnCommandBufferBeginRecording_0},
			runtime_replace{"nvnCommandBufferReportCounter", (uintptr_t*)&nvnCommandBufferReportCounter_0},
			runtime_replace{"nvnCommandBufferEndRecording", (uintptr_t*)&nvnCommandBufferEndRecording_0},
			runtime_replace{"nvnQueueSubmitCommands", (uintptr_t*)&nvnQueueSubmitCommands_0},
			runtime_replace{"nvnQueueFenceSync", (uintptr_t*)&nvnQueueFenceSync_0},
			runtime_replace{"nvnCommandBufferResetCounter", (uintptr_t*)&nvnCommandBufferResetCounter_0},
			runtime_replace{"nvnTextureBuilderSetDevice", (uintptr_t*)&nvnTextureBuilderSetDevice_0},
			runtime_replace{"nvnTextureBuilderSetDefaults", (uintptr_t*)&nvnTextureBuilderSetDefaults_0},
			runtime_replace{"nvnTextureBuilderSetFlags", (uintptr_t*)&nvnTextureBuilderSetFlags_0},
			runtime_replace{"nvnTextureBuilderSetTarget", (uintptr_t*)&nvnTextureBuilderSetTarget_0},
			runtime_replace{"nvnTextureBuilderSetFormat", (uintptr_t*)&nvnTextureBuilderSetFormat_0},
			runtime_replace{"nvnTextureBuilderSetSize2D", (uintptr_t*)&nvnTextureBuilderSetSize2D_0},
			runtime_replace{"nvnTextureBuilderGetStorageSize", (uintptr_t*)&nvnTextureBuilderGetStorageSize_0},
			runtime_replace{"nvnTextureBuilderGetStorageAlignment", (uintptr_t*)&nvnTextureBuilderGetStorageAlignment_0},
			runtime_replace{"nvnTextureBuilderSetStorage", (uintptr_t*)&nvnTextureBuilderSetStorage_0},
			runtime_replace{"nvnTextureInitialize", (uintptr_t*)&nvnTextureInitialize_0},
			runtime_replace{"nvnTextureGetFlags", (uintptr_t*)&nvnTextureGetFlags_0},
			runtime_replace{"nvnTextureGetTarget", (uintptr_t*)&nvnTextureGetTarget_0},
			runtime_replace{"nvnCommandBufferFinalize", (uintptr_t*)&nvnCommandBufferFinalize_0},
			runtime_replace{"nvnQueueWaitSync", (uintptr_t*)&nvnQueueWaitSync_0},
			runtime_replace{"nvnCommandBufferBarrier", (uintptr_t*)&nvnCommandBufferBarrier_0},
			runtime_replace{"nvnCommandBufferCopyTextureToTexture", (uintptr_t*)&nvnCommandBufferCopyTextureToTexture_0},
			runtime_replace{"nvnWindowGetCrop", (uintptr_t*)&nvnWindowGetCrop_0},
			runtime_replace{"nvnCommandBufferSetTexturePool", (uintptr_t*)&nvnCommandBufferSetTexturePool_0, (void*)CommandBufferSetTexturePool},
			runtime_replace{"nvnCommandBufferSetSamplerPool", (uintptr_t*)&nvnCommandBufferSetSamplerPool_0, (void*)CommandBufferSetSamplerPool}
		};

		for (const auto& replacement : nvn_replacements) {
			if (!strcmp(replacement.name, nvnFunction)) {
				if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
				if (replacement.hook_ptr) return (uintptr_t)replacement.hook_ptr;
				break;
			}
		}
		return address;
	}

	uintptr_t BootstrapLoader_1(const char* nvnName) {
		if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
			Shared->API = 1;
			if (!NVN::nvnDeviceGetProcAddress_0) {
				auto temp = nvnBootstrapLoader_0("nvnDeviceGetProcAddress");
				memcpy(&NVN::nvnDeviceGetProcAddress_0, &temp, sizeof(temp));
				static_assert(sizeof(temp) == sizeof(void*));
			}
			return (uintptr_t)&GetProcAddress0;
		}
		return nvnBootstrapLoader_0(nvnName);
	}
}
