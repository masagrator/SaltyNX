#pragma once

#if defined(SWITCH32)
#include <switch_min.h>
#elif defined(SWITCH)
#include <switch.h>
#else
#error "Unsupported base architecture!"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>

#define NOINLINE __attribute__ ((noinline))

#if defined(SWITCH)
#define PACKED NX_PACKED
#endif

#if defined(SWITCH) || defined(OUNCE)
namespace Utils {
	uint64_t _convertToTimeSpan(uint64_t tick);
}

namespace nn {
	Result SetUserInactivityDetectionTimeExtended(bool isTrue);
}
#endif

namespace LOCK {

	enum class Region : uint8_t {
		Absolute  = 0,
		Main      = 1,
		Heap      = 2,
		Alias     = 3,
		#if defined(SWITCH) || defined(OUNCE)
		Variables = 4,
		CodeCave  = 5,
		#endif
	};

	enum class ValueType : uint8_t {
		U8  = 0x01, U16 = 0x02, U32 = 0x04, U64 = 0x08,
		S8  = 0x11, S16 = 0x12, S32 = 0x14, S64 = 0x18,
		F32 = 0x24, F64 = 0x28,
		RefreshRate = 0x38,
	};

	enum class CompareType : uint8_t {
		GT = 1,
		GE = 2,
		LT = 3,
		LE = 4,
		EQ = 5,
		NE = 6,
	};

	enum class CodeCaveAdjustmentType : uint8_t {
		None = 0,
		Branch_Direct = 1,
		Adrp_CodeCave = 2,
		Adrp_Variables = 3,
		Adrp_MainFromCodeCave = 4,
		Branch_Relative = 5,
	};

	enum class MasterWriteOpcode : uint8_t {
		Bytes = 1,
		Variables = 2,
		CodeCave = 3,
		End = 0xFF,
	};

	enum class AllFpsOpcode : uint8_t {
		Write = 1,
		Eval_Write = 0x81,
		Compare = 2,
		Eval_Compare = 0x82,
		Block = 3,
		End = 0xFF,
	};

	enum class BlockOpcodeWhatType : uint8_t {
		Timing = 1,
	};

	class Patcher {
	public:
		struct Mappings {
			intptr_t  main_start      = 0;
			uintptr_t alias_start     = 0;
			uintptr_t heap_start      = 0;
			intptr_t  variables_start = 0;
			intptr_t  codeCave_start  = 0;
		};

		// How long to wait for the display to settle when the docked refresh rate changes.
		static constexpr size_t DockedRefreshRateDelay = 4000000000;

		constexpr Patcher() = default;

		// Non-copyable: there is exactly one patcher, and it owns a heap buffer.
		Patcher(const Patcher&) = delete;
		Patcher& operator=(const Patcher&) = delete;

		Result loadFromFile(const char* path);
		void bindMainRegion(intptr_t main_start);
		void bindDynamicRegions(uintptr_t alias_start, uintptr_t heap_start);

		bool isHeaderValid(const uint8_t* buffer);
		Result applyMasterWrite(FILE* file, size_t master_offset);
		Result applyPatch(uint8_t FPS, uint8_t refreshRate = 60);

		bool hasMasterWrite() const      { return m_masterWrite != 0; }
		bool masterWriteApplied() const  { return m_masterWriteApplied; }

		bool fpsDelayBlocked() const     { return m_blockDelayFPS; }

		double refreshRateOverwrite() const { return m_overwriteRefreshRate; }
		void clearRefreshRateOverwrite()    { m_overwriteRefreshRate = 0; }

		intptr_t mainRegion() const      { return m_mappings.main_start; }

	private:
		class Cursor {
		public:
			constexpr Cursor() = default;
			constexpr Cursor(const uint8_t* data, size_t offset)
				: m_data(data), m_offset(offset) {}

			template <typename T> T read();

			const uint8_t* take(size_t bytes);

			const char* readString();

			constexpr size_t tell() const { return m_offset; }
			constexpr void seek(size_t offset) { m_offset = offset; }
			constexpr const uint8_t* data() const { return m_data; }

		private:
			const uint8_t* m_data = nullptr;
			size_t m_offset = 0;
		};

		class Writer {
		public:
			constexpr Writer() = default;
			constexpr Writer(uint8_t* data, size_t offset)
				: m_data(data), m_offset(offset) {}

			template <typename T> void write(T value);

			void copy(const uint8_t* src, size_t bytes);

			constexpr size_t tell() const { return m_offset; }

		private:
			uint8_t* m_data = nullptr;
			size_t m_offset = 0;
		};

		static constexpr uint8_t memberSize(ValueType value_type) { return (uint8_t)value_type % 0x10; }
		static constexpr bool validMemberSize(uint8_t size) {
			return size != 0 && size <= 8 && (size & (size - 1)) == 0;
		}

		template <typename T>
		static bool compareValues(T value1, T value2, CompareType compare_type);

		intptr_t NOINLINE getAddress(Cursor& cursor) const;

		Result processBytes(FILE* file);
#if defined(SWITCH) || defined(OUNCE)
		Result processVariables(FILE* file);
		Result processCodeCave(FILE* file);
#endif

		static double NOINLINE evaluateExpression(const char* equation, double fps_target,
		                                          double displaySync);
		static Result writeExprTo(double value, Writer& out, ValueType value_type);

		void copyAddress(Cursor& in, Writer& out) const;
		Result copyValues(Cursor& in, Writer& out, bool evaluate,
		                  uint8_t FPS, uint8_t refreshRate) const;

		Result NOINLINE convertPatchToFPSTarget(uint8_t* out_buffer, const uint8_t* in_buffer,
		                                        uint8_t FPS, uint8_t refreshRate);

		Result execWrite(Cursor& cursor);
		Result execCompare(Cursor& cursor);
		Result execBlock(Cursor& cursor);

		Mappings m_mappings{};

		uint8_t  m_gen                  = 3;
		uint8_t  m_masterWrite          = 0;
		bool     m_unsafeCheck          = false;
		bool     m_masterWriteApplied   = false;
		uint32_t m_compiledSize         = 0;

		bool     m_blockDelayFPS        = false;
		double   m_overwriteRefreshRate = 0;

		uint8_t* m_compiled             = nullptr;
		uint8_t  m_compiledFPS          = 0;
		uint8_t  m_compiledRefreshRate  = 0;
		uint8_t* m_configBuffer           = nullptr;
	};
}
