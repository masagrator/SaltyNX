#include "lock.hpp"
#include "tinyexpr/tinyexpr.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

#ifdef SWITCH_BUILD
#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "useful.h"
#endif

#ifdef SWITCH_64BIT
extern "C" void codeCave();
//We need to define something in that section and reference its pointer to not get whole section discarded by garbage collector
//Trick to get section page aligned to 0x1000 with size 0x1000 without using linker script
__asm__(
    ".section .codecave, \"ax\", %progbits\n"
    ".global codeCave\n"
    ".type codeCave, %function\n"
    ".align 12\n"

    "codeCave:\n"
    "    nop\n"
    "    ret\n"

    ".align 12\n"
);

alignas(0x1000) static uint8_t variables_buffer[0x1000];
#endif

#ifdef HOST_BUILD
// ---------------------------------------------------------------------------
// Host validator sandbox - compiled only with -DHOST_BUILD, absent on hardware.
//
// Layout (one MAP_NORESERVE reservation; untouched pages cost nothing):
//
//   offset        size       purpose
//   0x00000000    256 MiB    low guard - absorbs negative offsets
//   0x10000000      2 GiB    Region::Main
//   0x90000000    256 MiB    Region::Heap
//   0xA0000000    256 MiB    Region::Alias
//   0xB0000000    512 MiB    pointee arena for materialised pointer targets
//   0xD0000000      4 KiB    Region::Variables (page aligned)
//   0xD0001000      4 KiB    Region::CodeCave  (page aligned; the Variables page
//                            below it keeps codeCave_start - 0x100 mapped)
//   0xD0002000    256 MiB    high guard
// ---------------------------------------------------------------------------

#include <sys/mman.h>
#include <cstdarg>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Host {
namespace {

	constexpr size_t MiB = 1024ull * 1024ull;

	constexpr size_t GUARD_LO_OFF  = 0;
	constexpr size_t GUARD_LO_SIZE = 256 * MiB;

	constexpr size_t MAIN_OFF      = GUARD_LO_OFF + GUARD_LO_SIZE;   // 0x10000000
	constexpr size_t MAIN_SIZE     = 2048 * MiB;

	constexpr size_t HEAP_OFF      = MAIN_OFF + MAIN_SIZE;           // 0x90000000
	constexpr size_t HEAP_SIZE     = 256 * MiB;

	constexpr size_t ALIAS_OFF     = HEAP_OFF + HEAP_SIZE;           // 0xA0000000
	constexpr size_t ALIAS_SIZE    = 256 * MiB;

	constexpr size_t POINTEE_OFF   = ALIAS_OFF + ALIAS_SIZE;         // 0xB0000000
	constexpr size_t POINTEE_SIZE  = 512 * MiB;

	constexpr size_t VARS_OFF      = POINTEE_OFF + POINTEE_SIZE;     // 0xD0000000
	constexpr size_t VARS_SIZE     = 0x1000;

	constexpr size_t CAVE_OFF      = VARS_OFF + VARS_SIZE;           // 0xD0001000
	constexpr size_t CAVE_SIZE     = 0x1000;

	constexpr size_t GUARD_HI_OFF  = CAVE_OFF + CAVE_SIZE;
	constexpr size_t GUARD_HI_SIZE = 256 * MiB;

	constexpr size_t TOTAL_SIZE    = GUARD_HI_OFF + GUARD_HI_SIZE;

	uint8_t* g_base = nullptr;
	bool     g_verbose = false;

	std::vector<std::string> g_errors;

	// Slot address -> the target that slot dereferences to. See loadPointer().
	std::unordered_map<intptr_t, intptr_t> g_pointees;

	// Materialised targets are spread across the pointee arena rather than
	// stacked on one address, so that data written through one chain does not
	// sit on top of another chain's target. The observed corpus applies offsets
	// of -27.5 MiB to +916 KiB after a dereference; a 4 MiB stride plus 64 MiB
	// of headroom below the first target keeps those landing inside the
	// reservation. The index wraps, which is harmless: colliding *data* is fine,
	// only colliding pointer bookkeeping was ever the problem.
	constexpr size_t POINTEE_HEADROOM = 64 * MiB;
	constexpr size_t POINTEE_STRIDE   = 4 * MiB;
	constexpr size_t POINTEE_SLOTS    = 96;

	inline intptr_t pointeeTarget(size_t index) {
		return reinterpret_cast<intptr_t>(
			g_base + POINTEE_OFF + POINTEE_HEADROOM +
			(index % POINTEE_SLOTS) * POINTEE_STRIDE);
	}

	// Cap the log so one pathological file cannot produce unbounded output.
	constexpr size_t MAX_ERRORS = 64;

	std::set<std::string> g_seen;

	void record(const std::string& msg) {
		// The patch is applied once per FPS/refresh-rate combination, so the same
		// underlying defect surfaces several times. Report each distinct problem
		// once; the count of combinations it affects adds nothing.
		if (!g_seen.insert(msg).second)
			return;
		if (g_errors.size() < MAX_ERRORS)
			g_errors.push_back(msg);
		else if (g_errors.size() == MAX_ERRORS)
			g_errors.push_back("(further problems suppressed)");
	}

	__attribute__((format(printf, 1, 2)))
	void recordf(const char* fmt, ...) {
		char buf[512];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		record(buf);
	}

	inline bool inSandbox(uintptr_t addr, size_t len) {
		if (!g_base) return false;
		const uintptr_t lo = reinterpret_cast<uintptr_t>(g_base);
		const uintptr_t hi = lo + TOTAL_SIZE;
		if (addr < lo || addr >= hi) return false;
		return len <= hi - addr;
	}
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

bool createSandbox() {
	if (g_base) return true;
	void* p = mmap(nullptr, TOTAL_SIZE, PROT_READ | PROT_WRITE,
	               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (p == MAP_FAILED) {
		g_base = nullptr;
		return false;
	}
	g_base = static_cast<uint8_t*>(p);
	return true;
}

void destroySandbox() {
	if (g_base) {
		munmap(g_base, TOTAL_SIZE);
		g_base = nullptr;
	}
}

intptr_t  mainRegion()      { return reinterpret_cast<intptr_t>(g_base + MAIN_OFF); }
uintptr_t heapRegion()      { return reinterpret_cast<uintptr_t>(g_base + HEAP_OFF); }
uintptr_t aliasRegion()     { return reinterpret_cast<uintptr_t>(g_base + ALIAS_OFF); }
intptr_t  variablesRegion() { return reinterpret_cast<intptr_t>(g_base + VARS_OFF); }
intptr_t  codeCaveRegion()  { return reinterpret_cast<intptr_t>(g_base + CAVE_OFF); }

// -------------------------------------------------------------------------
// Hooks used by lock.cpp
// -------------------------------------------------------------------------

bool isMapped(intptr_t addr, size_t len) {
	return inSandbox(static_cast<uintptr_t>(addr), len);
}

Result writeMemory(uintptr_t to, uintptr_t from, size_t size) {
	if (!inSandbox(to, size)) {
		reportBadWrite(to, size);
		return 0x1;
	}
	memcpy(reinterpret_cast<void*>(to), reinterpret_cast<const void*>(from), size);
	return 0;
}

intptr_t loadPointer(intptr_t addr) {
	if (!inSandbox(static_cast<uintptr_t>(addr), sizeof(intptr_t))) {
		reportBadRead(addr);
		// Hand back a usable target anyway: the caller may not be validating
		// this link, and returning garbage would turn a reportable problem into
		// a segfault that hides every later finding in the same file.
		return pointeeTarget(0);
	}

	// Deliberately NOT read out of sandbox memory.
	//
	// On hardware the game has already stored a real pointer at this slot, and
	// the patch's own writes never land on the game's pointer tables. Here the
	// slot starts out zeroed and the patch *does* write through these chains, so
	// reading memory back would let one chain's data (an evaluated FPS double,
	// say) be re-read as another chain's pointer. Keeping the mapping in a side
	// table makes every chain resolve to a stable, distinct target that patch
	// data can never clobber.
	auto it = g_pointees.find(addr);
	if (it != g_pointees.end())
		return it->second;

	const intptr_t target = pointeeTarget(g_pointees.size());
	g_pointees.emplace(addr, target);
	return target;
}

void logf(const char* fmt, ...) {
	if (!g_verbose) return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

// -------------------------------------------------------------------------
// Diagnostics
// -------------------------------------------------------------------------

void reportExpressionError(const char* equation, int error_pos) {
	recordf("expression does not parse (at character %d): \"%s\"", error_pos, equation);
}

void reportExpressionNotFinite(const char* equation, double value) {
	recordf("expression evaluates to %s: \"%s\"",
	        (value != value) ? "NaN" : (value > 0 ? "+infinity" : "-infinity"), equation);
}

void reportCompiledOverflow(size_t offset, size_t bytes, size_t capacity) {
	recordf("compiled patch overflows its buffer: writing %zu byte(s) at offset "
	        "%zu exceeds the %zu bytes reserved by the header",
	        bytes, offset, capacity);
}

void reportBadWrite(uintptr_t to, size_t size) {
	recordf("patch writes %zu byte(s) to 0x%llx, outside the emulated address space",
	        size, (unsigned long long)to);
}

void reportBadRead(intptr_t addr) {
	recordf("patch dereferences 0x%llx, outside the emulated address space",
	        (unsigned long long)addr);
}

void setVerbose(bool on) { g_verbose = on; }
bool verbose()           { return g_verbose; }

size_t errorCount() { return g_errors.size(); }

void printErrors(FILE* out, const char* prefix) {
	for (const auto& e : g_errors)
		fprintf(out, "%s%s\n", prefix, e.c_str());
}

void resetErrors() { g_errors.clear(); g_seen.clear(); }

}


// Symbols processCodeCave() takes the address of when relocating CodeCave
// branches. On hardware these are real SaltyNX routines; the validator only
// needs them to exist so that `&Utils::_convertToTimeSpan` and friends yield a
// sane address for the branch-offset arithmetic. Nothing calls them.
namespace Utils {
	uint64_t _convertToTimeSpan(uint64_t tick) { return tick; }
}

namespace nn {
	Result SetUserInactivityDetectionTimeExtended(bool isTrue) { (void)isTrue; return 0; }
}

#endif // HOST_BUILD

namespace LOCK {

namespace {

	double TruncDec(double value, double truncator) {
		size_t factor = pow(10, truncator);
		return trunc(value*factor) / factor;
	}

	bool NOINLINE isAddressValid(patch_addr_t address_in) {

#ifdef SWITCH_BUILD
		int64_t address = address_in;
		MemoryInfo memoryinfo = {0};
		uint32_t pageinfo = 0;

	#if defined(SWITCH_64BIT)
		#define MIN_ASLR_ADDRESS 0x8000000
		#define MAX_ASLR_ADDRESS 0x7FFFFFFFFF
	#elif defined(SWITCH_32BIT)
		#define MIN_ASLR_ADDRESS 0x200000
		#define MAX_ASLR_ADDRESS 0xFFFFFFFF
	#endif
		if (address < MIN_ASLR_ADDRESS || address > MAX_ASLR_ADDRESS) return false;

		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, address);
		if (R_FAILED(rc)) return false;
		if ((memoryinfo.perm & Perm_Rw) && ((address - memoryinfo.addr >= 0) && (address - memoryinfo.addr <= memoryinfo.size)))
			return true;
		return false;
#elif defined(HOST_BUILD)
		// The sandbox is the stand-in for the game's address space: an address is
		// valid exactly when it lands inside one of the emulated regions.
		return Host::isMapped(address_in, sizeof(patch_addr_t));
#else
		#error "isAddressValid function is not defined!"
#endif
	}

	inline FILE* fopen_sdcard(const char* path, const char* mode) {
		#ifdef SWITCH_BUILD
		return SaltySDCore_fopen(path, mode);
		#else
		return fopen(path, mode);
		#endif
	}

	inline size_t fread_sdcard(void* ptr, size_t size, size_t count, FILE* stream) {
		#ifdef SWITCH_BUILD
		return SaltySDCore_fread(ptr, size, count, stream);
		#else
		return fread(ptr, size, count, stream);
		#endif
	}

	inline int fseek_sdcard(FILE* stream, int64_t offset, int origin) {
		#ifdef SWITCH_BUILD
		return SaltySDCore_fseek(stream, offset, origin);
		#else
		return fseek(stream, offset, origin);
		#endif
	}

	inline size_t ftell_sdcard(FILE* stream) {
		#ifdef SWITCH_BUILD
		return SaltySDCore_ftell(stream); 
		#else
		return ftell(stream);
		#endif
	}

	inline int fclose_sdcard(FILE* stream) {
		#ifdef SWITCH_BUILD
		return SaltySDCore_fclose(stream); 
		#else
		return fclose(stream);
		#endif
	}

	#ifdef SWITCH_BUILD
	#define printf_sdcard SaltySDCore_printf
	#elif defined(HOST_BUILD)
	#define printf_sdcard Host::logf
	#else
	#error "printf_sdcard is not defined!"
	#endif

	inline Result memcpy_unsafe(uintptr_t to, uintptr_t from, size_t size) {
		#ifdef SWITCH_BUILD
		return SaltySD_Memcpy(to, from, size);
		#elif defined(HOST_BUILD)
		return Host::writeMemory(to, from, size);
		#else
		#error "memcpy_unsafe is not defined!"
		#endif
	}

#ifdef HOST_BUILD
	// execWrite/execCompare touch the target directly (on hardware the plugin
	// lives in the game's own address space, so there is no IPC involved). In
	// the validator a resolved address can fall outside the emulated space -
	// through a corrupt file, or simply an offset larger than the window we
	// reserve - and a raw memcpy would segfault with no diagnostic. Report and
	// bail instead.
	#define HOST_REQUIRE_MAPPED(addr, len) \
		do { \
			if (!Host::isMapped((intptr_t)(addr), (len))) { \
				Host::reportBadWrite((uintptr_t)(addr), (len)); \
				return 0x3008; \
			} \
		} while (0)
#else
	#define HOST_REQUIRE_MAPPED(addr, len) ((void)0)
#endif

	template <typename T>
	void outWriteType (auto& in, auto& out) {
		out.template write<T>(in.template read<T>());
	}

	void outWriteVal (auto in, auto& out) {
		out.template write<decltype(in)>(in);
	}

	#define OUT_TYPE(T) outWriteType<T>(in, out)
	#define OUT_VAL(in) outWriteVal(in, out)
	#define OUT_ADDRESS() copyAddress(in, out)
}


template <typename T>
T Patcher::Cursor::read() {
	T ret;
	memcpy(&ret, &m_data[m_offset], sizeof(T));
	m_offset += sizeof(T);
	return ret;
}

const uint8_t* Patcher::Cursor::take(size_t bytes) {
	const uint8_t* ptr = &m_data[m_offset];
	m_offset += bytes;
	return ptr;
}

const char* Patcher::Cursor::readString() {
	const char* str = reinterpret_cast<const char*>(&m_data[m_offset]);
	m_offset += strlen(str) + 1;
	return str;
}

#ifdef HOST_BUILD
bool Patcher::Writer::checkRoom(size_t bytes) {
	if (m_offset + bytes <= m_capacity)
		return true;
	if (!m_overflowed) {
		m_overflowed = true;
		Host::reportCompiledOverflow(m_offset, bytes, m_capacity);
	}
	return m_offset + bytes <= m_capacity + HOST_COMPILED_SLACK;
}
#define WRITER_CHECK(n) if (!checkRoom(n)) { m_offset += (n); return; }
#else
#define WRITER_CHECK(n) ((void)0)
#endif

template <typename T>
void Patcher::Writer::write(T value) {
	WRITER_CHECK(sizeof(T));
	memcpy(&m_data[m_offset], &value, sizeof(T));
	m_offset += sizeof(T);
}

void Patcher::Writer::copy(const uint8_t* src, size_t bytes) {
	WRITER_CHECK(bytes);
	// HOS requires from SIMD load/store instructions to have aligned pointers in A32 mode, so we must avoid using VSTR here
	memcpy(&m_data[m_offset], src, bytes);
	m_offset += bytes;
}

void Patcher::bindMainRegion(intptr_t main_start) {
	m_mappings.main_start = main_start;
#if defined(SWITCH_64BIT)
	m_mappings.variables_start = (intptr_t)&variables_buffer[0];
	m_mappings.codeCave_start  = (intptr_t)&codeCave;
#elif defined(HOST_BUILD)
	m_mappings.variables_start = Host::variablesRegion();
	m_mappings.codeCave_start  = Host::codeCaveRegion();
#endif
}

void Patcher::bindDynamicRegions(uintptr_t alias_start, uintptr_t heap_start) {
	m_mappings.alias_start = alias_start;
	m_mappings.heap_start  = heap_start;
}

template <typename T>
bool Patcher::compareValues(T value1, T value2, CompareType compare_type) {
    return [&]<typename... M>(std::tuple<M...>) { 
        return (... || (compare_type == M::val ? typename M::op{}(value1, value2) : false)); 
    }(CompareMappings{});
}

patch_addr_t NOINLINE Patcher::getAddress(Cursor& cursor) const {
	bool unsafe_address = !m_unsafeCheck;
	if (m_gen == 4) unsafe_address = cursor.read<bool>();
	int8_t offsets_count = cursor.read<int8_t>();
	Region region = cursor.read<Region>();
	offsets_count -= 1;

	if (region >= Region::Total)
		return -1;
	
	int64_t address = [&]<typename... M>(std::tuple<M...>) {
		return (... + (region == M::val ? static_cast<int64_t>(m_mappings.*M::ptr) : 0)); 
	}(RegionMappings{});

	for (int i = 0; i < offsets_count; i++) {
#if defined(LOCK_ABI32)
		int32_t temp_offset = cursor.read<int32_t>();
		address += temp_offset;
#elif defined(LOCK_ABI64)
		uint32_t temp_offset = cursor.read<uint32_t>();
		if (region > Region::Absolute && region < Region::Variables) {
			int32_t temp_offset_int = 0;
			memcpy(&temp_offset_int, &temp_offset, 4);
			address += (int64_t)temp_offset_int;
		}
		else address += (int64_t)temp_offset;
#endif
		if (i + 1 < offsets_count) {
#ifdef HOST_BUILD
			const patch_addr_t next = Host::loadPointer(address);
#else
			const patch_addr_t next = *(patch_addr_t*)address;
#endif
			if (unsafe_address && !isAddressValid(next)) return -2;
			address = next;
		}
	}
	return address;
}

bool Patcher::isHeaderValid(const uint8_t* buffer) {
	const uint8_t MAGIC[4] = {'L', 'O', 'C', 'K'};
	if (memcmp(buffer, MAGIC, sizeof(MAGIC)) != 0)
		return false;
	m_gen = buffer[4];
	if (m_gen < MIN_SUPPORTED_GEN || m_gen > MAX_SUPPORTED_GEN)
		return false;
	m_masterWrite = buffer[5];
	if (m_masterWrite > 1)
		return false;
	m_unsafeCheck = (bool)buffer[7];

	uint8_t start_offset = 0xC;
	if (m_masterWrite) start_offset += 4;
	uint32_t header_size = 0;
	memcpy(&header_size, &buffer[8], sizeof(header_size));
	if (header_size != start_offset)
		return false;

	m_compiledSize = (uint32_t)buffer[6] * buffer[6];
	return true;
}

Result Patcher::processBytes(FILE* file) {
	OpHeader header;
	fread_sdcard(&header, sizeof(OpHeader), 1, file);
	const auto member_size = memberSize(header.value_type);
	void* temp_buffer = calloc(header.elements, member_size);
	fread_sdcard(temp_buffer, member_size, header.elements, file);
	memcpy_unsafe(m_mappings.main_start + header.main_offset, (uintptr_t)temp_buffer, member_size * header.elements);
	free(temp_buffer);
	return 0;
}

#ifdef LOCK_ABI64

Result Patcher::processVariables(FILE* file) {
	OpHeader header;
	fread_sdcard(&header, sizeof(OpHeader), 1, file);
	const auto member_size = memberSize(header.value_type);
	void* temp_buffer = calloc(header.elements, member_size);
	fread_sdcard(temp_buffer, member_size, header.elements, file);
	memcpy_unsafe(m_mappings.variables_start + header.main_offset, (uintptr_t)temp_buffer, member_size * header.elements);
	free(temp_buffer);
	return 0;
}

Result Patcher::processCodeCave(FILE* file) {
	Region address_region{};
	fread_sdcard(&address_region, 1, 1, file);
	OpHeader header;
	fread_sdcard(&header, sizeof(OpHeader), 1, file);

	CodeCaveData* temp_buffer = (CodeCaveData*)calloc(header.elements, sizeof(CodeCaveData));
	uint32_t* output = 0;
	if (address_region == Region::CodeCave) output = (uint32_t*)(m_mappings.codeCave_start + header.main_offset);
	else if (address_region == Region::Main) output = (uint32_t*)(m_mappings.main_start + header.main_offset);
	else return 0x321;
	fread_sdcard(temp_buffer, sizeof(CodeCaveData), header.elements, file);
	for (size_t i = 0; i < header.elements; i++) {
		switch (temp_buffer[i].adjustment_type) {
			case CodeCaveAdjustmentType::None:
				memcpy_unsafe((uintptr_t)&output[i], (uintptr_t)&temp_buffer[i].instruction, 4);
				break;
			case CodeCaveAdjustmentType::Branch_Direct: {
				BranchOp Branch;
				memcpy(&Branch, &temp_buffer[i].instruction, 4);
				patch_addr_t current_address = (patch_addr_t)&output[i];
				if (Branch.imm == -1) {
					patch_addr_t jump_address = (patch_addr_t)&Utils::_convertToTimeSpan;
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				else if (Branch.imm == -2) {
					patch_addr_t jump_address = (patch_addr_t)&nn::SetUserInactivityDetectionTimeExtended;
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				else if (Branch.imm <= -64) {
					patch_addr_t jump_address = (m_mappings.codeCave_start - 0x100) + (((int64_t)(Branch.imm)*4) * -1);
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				else if (address_region == Region::CodeCave) {
					patch_addr_t jump_address = (patch_addr_t)(m_mappings.main_start + ((int64_t)(Branch.imm)*4 + (header.main_offset + (i*4))));
					current_address = (patch_addr_t)&output[i];
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				memcpy_unsafe((uintptr_t)&output[i], (uintptr_t)&Branch, 4);
				break;
			}
			case CodeCaveAdjustmentType::Adrp_CodeCave: [[fallthrough]];
			case CodeCaveAdjustmentType::Adrp_Variables: [[fallthrough]];
			case CodeCaveAdjustmentType::Adrp_MainFromCodeCave: {
				struct {
					unsigned int reg: 5;
					signed int immhi: 19;
					unsigned int reserved: 5;
					unsigned int immlo: 2;
					bool op: 1;
				} ADRP;
				static_assert(sizeof(ADRP) == 4);

				memcpy(&ADRP, &temp_buffer[i].instruction, 4);
				patch_addr_t current_address = (patch_addr_t)(&output[i]) & ~0xFFF;
				patch_addr_t jump_address = 0;
				ptrdiff_t offset = 0;
				switch(temp_buffer[i].adjustment_type) {
					case CodeCaveAdjustmentType::Adrp_CodeCave: 
						jump_address = (patch_addr_t)m_mappings.codeCave_start; offset = jump_address - current_address; break;
					case CodeCaveAdjustmentType::Adrp_Variables: 
						jump_address = (patch_addr_t)m_mappings.variables_start; offset = jump_address - current_address; break;
					case CodeCaveAdjustmentType::Adrp_MainFromCodeCave:
						jump_address = (patch_addr_t)(((uintptr_t)ADRP.immlo << 12) + ((uintptr_t)ADRP.immhi << 14)); 
						offset = jump_address + (m_mappings.main_start - m_mappings.codeCave_start); break;
					default: return 0x346;
				}
				ADRP.immlo = (offset % 0x4000) >> 12;
				ADRP.immhi = (offset >> 14);
				memcpy_unsafe((uintptr_t)&output[i], (uintptr_t)&ADRP, 4);
				break;
			}
			case CodeCaveAdjustmentType::Branch_Relative: {
				BranchOp Branch;
				memcpy(&Branch, &temp_buffer[i].instruction, 4);
				patch_addr_t current_address = (patch_addr_t)&output[i];
				patch_addr_t jump_address = (patch_addr_t)(m_mappings.main_start + ((int64_t)(Branch.imm)*4) + (i*4));
				ptrdiff_t offset = jump_address - current_address;
				Branch.imm = offset / 4;
				memcpy_unsafe((uintptr_t)&output[i], (uintptr_t)&Branch, 4);
				break;
			}
			default:
				return 0x345;
		}
	}
	free(temp_buffer);
	return 0;
}

#endif

Result Patcher::applyMasterWrite(FILE* file, size_t master_offset) {
	uint32_t offset_impl = 0;

	fseek_sdcard(file, master_offset, 0);
	fread_sdcard(&offset_impl, 4, 1, file);
	fseek_sdcard(file, offset_impl, 0);
	if (ftell_sdcard(file) != offset_impl)
		return 0x312;

	MasterWriteOpcode OPCODE{};
	while (true) {
		if (fread_sdcard(&OPCODE, 1, 1, file) != 1)
			return 0x313;
		printf_sdcard("LOCK: processes opcode: %d, offset: 0x%lx\n", (int)OPCODE, (unsigned long)ftell_sdcard(file));
		if (OPCODE == MasterWriteOpcode::End) {m_masterWriteApplied = true; return 0;}
		Result rc = 0xFF;
		[&]<typename... M>(std::tuple<M...>) {
			((OPCODE == M::val && (rc = (this->*M::func)(file), true)) || ...);
		}(MasterWriteMappings{});
		if (R_FAILED(rc)) return rc;
	}
}

Result Patcher::writeExprTo(double value, Writer& out, ValueType value_type) {
	union {
		uint64_t u;
		int64_t i;
		double d;
		float f;
	} tmp;

	switch (value_type) {
		case ValueType::F64: [[fallthrough]];
		case ValueType::RefreshRate:
			tmp.d = value;
			break;
		case ValueType::F32:
			tmp.f = (float)value;	
			break;
		default:
			switch(uint8_t(value_type) >> 4) {
				case 0: //unsigned
					tmp.u = (uint64_t)value;
					break;
				case 1: //signed
					tmp.i = (int64_t)value;
					break;
				default: return 0x4;
			}
	}

	out.copy((uint8_t*)(&tmp), memberSize(value_type));
	return 0;
}

double NOINLINE Patcher::evaluateExpression(const char* equation, double fps_target, double displaySync) {
	if (displaySync == 0) {
		displaySync = 60;
	}
	double FPS_TARGET = fps_target;
	double FPS_LOCK_TARGET = fps_target;
	if (fps_target >= displaySync) FPS_LOCK_TARGET += 2;
	double FRAMETIME_TARGET = 1000.0 / fps_target;
	double VSYNC_TARGET = (fps_target <= 60) ? trunc(60 / fps_target) : 1.0;
	double INTERVAL_TARGET = (fps_target <= displaySync) ? trunc(displaySync / fps_target) : 1.0;
	double REFRESH_RATE = displaySync;
	te_variable vars[] = {
		{"TruncDec", (const void*)TruncDec, TE_FUNCTION2},
		{"FPS_TARGET", &FPS_TARGET, TE_VARIABLE},
		{"FPS_LOCK_TARGET", &FPS_LOCK_TARGET, TE_VARIABLE},
		{"FRAMETIME_TARGET", &FRAMETIME_TARGET, TE_VARIABLE},
		{"VSYNC_TARGET", &VSYNC_TARGET, TE_VARIABLE},
		{"INTERVAL_TARGET", &INTERVAL_TARGET, TE_VARIABLE},
		{"REFRESH_RATE", &REFRESH_RATE, TE_VARIABLE}
	};
#ifdef HOST_BUILD
	int error_pos = 0;
	te_expr *n = te_compile(equation, vars, std::size(vars), &error_pos);
	if (!n) {
		Host::reportExpressionError(equation, error_pos);
		return 0;
	}
	double evaluated_value = te_eval(n);
	te_free(n);
	if (!std::isfinite(evaluated_value))
		Host::reportExpressionNotFinite(equation, evaluated_value);
	return evaluated_value;
#else
	te_expr *n = te_compile(equation, vars, std::size(vars), 0);
	double evaluated_value = te_eval(n);
	te_free(n);
	return evaluated_value;
#endif
}

void Patcher::copyAddress(Cursor& in, Writer& out) const {
	if (m_gen >= MAX_SUPPORTED_GEN) OUT_TYPE(uint8_t);
	uint8_t address_count = in.read<uint8_t>();
	OUT_VAL(address_count);
	OUT_TYPE(Region);
	for (size_t i = 1; i < address_count; i++) {
		OUT_TYPE(uint32_t);
	}
}

Result Patcher::copyValues(Cursor& in, Writer& out, bool evaluate, uint8_t FPS, uint8_t refreshRate) const {
	ValueType value_type = in.read<ValueType>();
	OUT_VAL(value_type);
	uint8_t value_count = in.read<uint8_t>();
	OUT_VAL(value_count);

	if (!evaluate) {
		const auto array_size = memberSize(value_type) * value_count;
		out.copy(in.take(array_size), array_size);
		return 0;
	}

	for (size_t i = 0; i < value_count; i++) {
		const double evaluated_value = evaluateExpression(in.readString(), (double)FPS, (double)refreshRate);
		Result rc = writeExprTo(evaluated_value, out, value_type);
		if (R_FAILED(rc)) return rc;
	}
	return 0;
}

Result NOINLINE Patcher::convertPatchToFPSTarget(uint8_t* out_buffer, const uint8_t* in_buffer, uint8_t FPS, uint8_t refreshRate) {
	uint32_t header_size = 0;
	memcpy(&header_size, &in_buffer[8], 4);
	memcpy(out_buffer, in_buffer, header_size);

	Cursor in(in_buffer, header_size);
#ifdef HOST_BUILD
	Writer out(out_buffer, header_size, m_compiledSize);
#else
	Writer out(out_buffer, header_size);
#endif

	while (true) {
		auto OPCODE = in.read<AllFpsOpcode>();
		bool evaluate = false;
		switch (OPCODE) {
			case AllFpsOpcode::Eval_Write:
				OPCODE = AllFpsOpcode::Write;
				evaluate = true;
				[[fallthrough]];
			case AllFpsOpcode::Write: {
				OUT_VAL(OPCODE);
				OUT_ADDRESS();
				Result rc = copyValues(in, out, evaluate, FPS, refreshRate);
				if (R_FAILED(rc)) return rc;
				break;
			}
			case AllFpsOpcode::Eval_Compare:
				OPCODE = AllFpsOpcode::Compare;
				evaluate = true;
				[[fallthrough]];
			case AllFpsOpcode::Compare: {
				OUT_VAL(OPCODE);
				OUT_ADDRESS();
				OUT_TYPE(CompareType);
				const auto value_type = in.read<ValueType>();
				OUT_VAL(value_type);
				const auto member_size = memberSize(value_type);
				out.copy(in.take(member_size), member_size);
				OUT_ADDRESS();
				Result rc = copyValues(in, out, evaluate, FPS, refreshRate);
				if (R_FAILED(rc)) return rc;
				break;
			}
			case AllFpsOpcode::Block:
				OUT_VAL(OPCODE);
				OUT_TYPE(BlockOpcodeWhatType);
				break;
			case AllFpsOpcode::End:
				OUT_VAL(OPCODE);
				return 0;
			default:
				return 0x2002;
		}
	}
}

Result Patcher::execWrite(Cursor& cursor) {
	patch_addr_t address = getAddress(cursor);
#ifdef LOCK_ABI64
	if (address < 0) return 0x6;
#endif

	const ValueType value_type = cursor.read<LOCK::ValueType>();
	const auto loops = cursor.read<uint8_t>();

	if (value_type == ValueType::RefreshRate) {
		for (uint8_t i = 0; i < loops; i++) {
			m_overwriteRefreshRate = cursor.read<double>();
		}
		return 0;
	}

	if (!address) return 0x3007;
	const auto member_size = memberSize(value_type);
	if (!validMemberSize(member_size))
		return 3;
	const auto array_size = member_size * loops;
	HOST_REQUIRE_MAPPED(address, array_size);
	memcpy((void*)address, cursor.take(array_size), array_size);
	return 0;
}

Result Patcher::execCompare(Cursor& cursor) {
	patch_addr_t address = getAddress(cursor);
#ifdef LOCK_ABI64
	if (address < 0) return 0x6;
#endif

	const auto compare_type = cursor.read<CompareType>();
	ValueType value_type = cursor.read<ValueType>();
	bool passed = false;

	// The fold below dereferences `address`; make sure that is safe first.
	HOST_REQUIRE_MAPPED(address, memberSize(value_type) ? memberSize(value_type) : 1);

	bool found = [&]<typename... M>(std::tuple<M...>) { 
		return (... || (value_type == M::val ? (passed = compareValues(*(typename M::type*)(address), cursor.read<typename M::type>(), compare_type), true) : false)); 
	} (TypeMappings{});
	if (!found) return 0x8;

	address = getAddress(cursor);
#ifdef LOCK_ABI64
	if (address < 0) return 0x6;
#endif
	value_type = cursor.read<ValueType>();
	const auto loops = cursor.read<uint8_t>();

	if (value_type == ValueType::RefreshRate) {
		for (uint8_t i = 0; i < loops; i++) {
			const auto valueDouble = cursor.read<uint64_t>();
			if (passed) memcpy(&m_overwriteRefreshRate, &valueDouble, sizeof(valueDouble));
		}
		return 0;
	}

	if (!address) return 0x3007;
	const auto member_size = memberSize(value_type);
	if (!validMemberSize(member_size))
		return 0x9;
	const auto array_size = member_size * loops;
	const auto source = cursor.take(array_size);
	if (passed) {
		HOST_REQUIRE_MAPPED(address, array_size);
		memcpy((void*)address, source, array_size);
	}
	return 0;
}

Result Patcher::execBlock(Cursor& cursor) {
	switch (cursor.read<BlockOpcodeWhatType>()) {
		case BlockOpcodeWhatType::Timing:
			m_blockDelayFPS = true;
			return 0;
		default:
			return 0x7;
	}
}

Result Patcher::applyPatch(uint8_t FPS, uint8_t refreshRate) {
	m_overwriteRefreshRate = 0;
	m_blockDelayFPS = false;
	if (!refreshRate) refreshRate = 60;

	if ((m_compiledFPS != FPS) || (m_compiledRefreshRate != refreshRate)) {
		if (m_compiled != nullptr) {
			free(m_compiled);
		}
#ifdef HOST_BUILD
		// Padding so that an undersized m_compiledSize is reported rather than smashing the heap.
		m_compiled = (uint8_t*)malloc(m_compiledSize + HOST_COMPILED_SLACK);
#else
		m_compiled = (uint8_t*)malloc(m_compiledSize);
#endif
		if (!m_compiled)
			return 0x3004;
		if (R_FAILED(convertPatchToFPSTarget(m_compiled, m_configBuffer, FPS, refreshRate))) {
			m_compiledFPS = 0;
			return 0x3002;
		}
		m_compiledFPS = FPS;
		m_compiledRefreshRate = refreshRate;
	}
	if (!m_compiled) {
		return 0x3003;
	}

	uint32_t start_offset = 0;
	memcpy(&start_offset, &m_compiled[0x8], sizeof(start_offset));
	Cursor cursor(m_compiled, start_offset);
	while (true) {
		const auto OPCODE = cursor.read<AllFpsOpcode>();
		if (OPCODE == AllFpsOpcode::End) return 0;
		Result rc = 0xFF;
		[&]<typename... M>(std::tuple<M...>) {
			((OPCODE == M::val && (rc = (this->*M::func)(cursor), true)) || ...);
		}(PostAllFpsMappings{});
		if (R_FAILED(rc)) return rc;
	}
}

Result Patcher::loadFromFile(const char* path) {
	FILE* patch_file = fopen_sdcard(path, "rb");
	if (!patch_file) {
		printf_sdcard("LOCK: could not open patch file!\n");
		return 0x1200;
	}
	fseek_sdcard(patch_file, 0, 2);
	size_t configSize = ftell_sdcard(patch_file);
	fseek_sdcard(patch_file, 8, 0);
	uint32_t header_size = 0;
	fread_sdcard(&header_size, 0x4, 1, patch_file);
	uint8_t* buffer = (uint8_t*)calloc(1, header_size);
	fseek_sdcard(patch_file, 0, 0);
	fread_sdcard(buffer, header_size, 1, patch_file);
	bool error = false;
	size_t tell = ftell_sdcard(patch_file);
	if (tell != header_size) {
		printf_sdcard("LOCK: wrong header! Expected: 0x%lx, got: 0x%lx\n", (unsigned long)header_size, (unsigned long)tell);
		error = true;
	}
	else if (!isHeaderValid(buffer)) {
		printf_sdcard("LOCK: file is invalid!\n");
		error = true;			
	}
	if (error == true) {
		fclose_sdcard(patch_file);
		free(buffer);
		return 0x1201;
	}
	if (hasMasterWrite()) {
		Result ret = applyMasterWrite(patch_file, header_size - 4);
		if (R_FAILED(ret))  {
			fclose_sdcard(patch_file);
			return ret;
		}
		configSize = *(uint32_t*)(&(buffer[header_size - 4]));
	}
	free(buffer);
	buffer = (uint8_t*)calloc(1, configSize);
	fseek_sdcard(patch_file, 0, 0);
	fread_sdcard(buffer, configSize, 1, patch_file);
	fclose_sdcard(patch_file);
	m_configBuffer = buffer;
	return 0;
}

}
