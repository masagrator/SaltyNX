#pragma once

#if defined(SWITCH32) || defined(OUNCE32)
	#define SWITCH_BUILD
	#define SWITCH_32BIT
	#define LOCK_ABI32
#elif defined(SWITCH) || defined(OUNCE)
	#define SWITCH_BUILD
	#define SWITCH_64BIT
	#define LOCK_ABI64
#elif defined(HOST_BUILD)
	#define LOCK_ABI64
#else
	#error "Unsupported base architecture!"
#endif

#if defined(SWITCH32)
#include <switch_min.h>
#elif defined(SWITCH)
#include <switch.h>
#elif defined(SWITCH_BUILD)
#error "OUNCE/OUNCE32 are placeholders and cannot be built yet!"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <functional>

#ifdef HOST_BUILD
#ifndef LOCK_HOST_RESULT_DEFINED
#define LOCK_HOST_RESULT_DEFINED
typedef uint32_t Result;
#endif
#ifndef R_FAILED
#define R_FAILED(res)      ((res)!=0)
#endif
#ifndef R_SUCCEEDED
#define R_SUCCEEDED(res)   ((res)==0)
#endif

namespace Host {

	bool createSandbox();
	void destroySandbox();

	intptr_t  mainRegion();
	uintptr_t heapRegion();
	uintptr_t aliasRegion();
	intptr_t  variablesRegion();
	intptr_t  codeCaveRegion();

	bool isMapped(intptr_t addr, size_t len);
	Result writeMemory(uintptr_t to, uintptr_t from, size_t size);
	intptr_t loadPointer(intptr_t addr);

	__attribute__((format(printf, 1, 2)))
	void logf(const char* fmt, ...);

	void reportExpressionError(const char* equation, int error_pos);
	void reportExpressionNotFinite(const char* equation, double value);
	void reportCompiledOverflow(size_t offset, size_t bytes, size_t capacity);
	void reportBadWrite(uintptr_t to, size_t size);
	void reportBadRead(intptr_t addr);

	void setVerbose(bool on);
	bool verbose();

	size_t errorCount();
	void printErrors(FILE* out, const char* prefix);
	void resetErrors();
}
#endif

#ifndef NOINLINE
#define NOINLINE __attribute__ ((noinline))
#endif

#if defined(SWITCH)
#define PACKED NX_PACKED
#elif defined(HOST_BUILD)
#define PACKED __attribute__((packed))
#endif

#ifdef LOCK_ABI64
namespace Utils {
	uint64_t _convertToTimeSpan(uint64_t tick);
}

namespace nn {
	Result SetUserInactivityDetectionTimeExtended(bool isTrue);
}
#endif

#define MIN_SUPPORTED_GEN 3
#define MAX_SUPPORTED_GEN 4

namespace LOCK {

#ifdef LOCK_ABI64
	using patch_addr_t = intptr_t;
#else
	using patch_addr_t = uintptr_t;
#endif

	enum class Region : uint8_t {
		Absolute,
		Main,
		Heap,
		Alias,
#ifdef LOCK_ABI64
		Variables,
		CodeCave,
#endif
		Total
	};
	static_assert(sizeof(Region) == 1);

	enum class ValueType : uint8_t {
		U8  = 0x01, U16 = 0x02, U32 = 0x04, U64 = 0x08,
		S8  = 0x11, S16 = 0x12, S32 = 0x14, S64 = 0x18,
		F32 = 0x24, F64 = 0x28,
		RefreshRate = 0x38,
	};
	static_assert(sizeof(ValueType) == 1);

	template<ValueType V, typename T> struct ValueMap { static constexpr ValueType val = V; using type = T; };

	using TypeMappings = std::tuple<
		ValueMap<ValueType::U8, uint8_t>, ValueMap<ValueType::U16, uint16_t>, ValueMap<ValueType::U32, uint32_t>, ValueMap<ValueType::U64, uint64_t>,
		ValueMap<ValueType::S8, int8_t>,  ValueMap<ValueType::S16, int16_t>,  ValueMap<ValueType::S32, int32_t>,  ValueMap<ValueType::S64, int64_t>,
		ValueMap<ValueType::F32, float>,  ValueMap<ValueType::F64, double>
	>;

	enum class CompareType : uint8_t {
		GT = 1, GE = 2, LT = 3, LE = 4, EQ = 5, NE = 6,
	};
	static_assert(sizeof(CompareType) == 1);

	template<CompareType C, typename Op> struct CmpMap { static constexpr CompareType val = C; using op = Op; };

	using CompareMappings = std::tuple<
		CmpMap<CompareType::GT, std::greater<>>,
		CmpMap<CompareType::GE, std::greater_equal<>>,
		CmpMap<CompareType::LT, std::less<>>,
		CmpMap<CompareType::LE, std::less_equal<>>,
		CmpMap<CompareType::EQ, std::equal_to<>>,
		CmpMap<CompareType::NE, std::not_equal_to<>>
	>;

	enum class CodeCaveAdjustmentType : uint8_t {
		None = 0,
		Branch_Direct = 1,
		Adrp_CodeCave = 2,
		Adrp_Variables = 3,
		Adrp_MainFromCodeCave = 4,
		Branch_Relative = 5,
	};
	static_assert(sizeof(CodeCaveAdjustmentType) == 1);

	enum class MasterWriteOpcode : uint8_t {
		Bytes = 1,
#ifdef LOCK_ABI64
		Variables = 2,
		CodeCave = 3,
#endif
		End = 0xFF,
	};
	static_assert(sizeof(MasterWriteOpcode) == 1);

	enum class AllFpsOpcode : uint8_t {
		Write = 1,
		Eval_Write = 0x81,
		Compare = 2,
		Eval_Compare = 0x82,
		Block = 3,
		End = 0xFF,
	};
	static_assert(sizeof(AllFpsOpcode) == 1);

	enum class BlockOpcodeWhatType : uint8_t {
		Timing = 1,
	};
	static_assert(sizeof(BlockOpcodeWhatType) == 1);

	struct OpHeader {
		uint32_t main_offset;
		ValueType value_type;
		uint8_t elements;
	} PACKED;
	static_assert(sizeof(OpHeader) == 6);

	struct CodeCaveData {
		CodeCaveAdjustmentType adjustment_type;
		uint32_t instruction;
	} PACKED;
	static_assert(sizeof(CodeCaveData) == 5);

	struct BranchOp {
		signed int imm: 26;
		unsigned int opcode: 6;
	};
	static_assert(sizeof(BranchOp) == 4);

#ifdef HOST_BUILD
	// Padding added to the compiled buffer by the validator only.
	static constexpr size_t HOST_COMPILED_SLACK = 64 * 1024;
#endif

	class Patcher {
	public:
		struct Mappings {
			intptr_t  main_start      = 0;
			uintptr_t alias_start     = 0;
			uintptr_t heap_start      = 0;
			intptr_t  variables_start = 0;
			intptr_t  codeCave_start  = 0;
		};

		template<Region R, auto Ptr> struct RegMap { static constexpr Region val = R; static constexpr auto ptr = Ptr; };

		using RegionMappings = std::tuple<
			RegMap<Region::Main,      &Mappings::main_start>,
			RegMap<Region::Heap,      &Mappings::heap_start>,
			RegMap<Region::Alias,     &Mappings::alias_start>
#ifdef LOCK_ABI64
		   	,
			RegMap<Region::Variables, &Mappings::variables_start>,
			RegMap<Region::CodeCave,  &Mappings::codeCave_start>
#endif
		>;

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
#ifdef HOST_BUILD
			constexpr Writer(uint8_t* data, size_t offset, size_t capacity)
				: m_data(data), m_offset(offset), m_capacity(capacity) {}

			bool checkRoom(size_t bytes);
#endif

			template <typename T> void write(T value);

			void copy(const uint8_t* src, size_t bytes);

			constexpr size_t tell() const { return m_offset; }

		private:
			uint8_t* m_data = nullptr;
			size_t m_offset = 0;
#ifdef HOST_BUILD
			size_t m_capacity = SIZE_MAX;
			bool   m_overflowed = false;
#endif
		};

		static constexpr uint8_t memberSize(ValueType value_type) { return (uint8_t)value_type % 0x10; }
		static constexpr bool validMemberSize(uint8_t size) {
			return size != 0 && size <= 8 && (size & (size - 1)) == 0;
		}

		template <typename T>
		static bool compareValues(T value1, T value2, CompareType compare_type);

		patch_addr_t NOINLINE getAddress(Cursor& cursor) const;

		Result processBytes(FILE* file);
#ifdef LOCK_ABI64
		Result processVariables(FILE* file);
		Result processCodeCave(FILE* file);
#endif

		template<MasterWriteOpcode C, auto Func> struct MasterWriteOpMap { static constexpr MasterWriteOpcode val = C; static constexpr auto func = Func; };

		using MasterWriteMappings = std::tuple<
			MasterWriteOpMap<MasterWriteOpcode::Bytes, &Patcher::processBytes>
#ifdef LOCK_ABI64
			,
			MasterWriteOpMap<MasterWriteOpcode::Variables, &Patcher::processVariables>,
			MasterWriteOpMap<MasterWriteOpcode::CodeCave, &Patcher::processCodeCave>
#endif
		>;

		static double NOINLINE evaluateExpression(const char* equation, double fps_target, double displaySync);
		static Result writeExprTo(double value, Writer& out, ValueType value_type);

		void copyAddress(Cursor& in, Writer& out) const;
		Result copyValues(Cursor& in, Writer& out, bool evaluate, uint8_t FPS, uint8_t refreshRate) const;

		Result NOINLINE convertPatchToFPSTarget(uint8_t* out_buffer, const uint8_t* in_buffer, uint8_t FPS, uint8_t refreshRate);

		Result execWrite(Cursor& cursor);
		Result execCompare(Cursor& cursor);
		Result execBlock(Cursor& cursor);

		template<AllFpsOpcode C, auto Func> struct AllFpsOpMap { static constexpr AllFpsOpcode val = C; static constexpr auto func = Func; };

		using PostAllFpsMappings = std::tuple<
			AllFpsOpMap<AllFpsOpcode::Write, &Patcher::execWrite>,
			AllFpsOpMap<AllFpsOpcode::Compare, &Patcher::execCompare>,
			AllFpsOpMap<AllFpsOpcode::Block, &Patcher::execBlock>
		>;

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
		uint8_t* m_configBuffer         = nullptr;
	};
}
