#include "lock.hpp"
#include "tinyexpr/tinyexpr.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "useful.h"

#if defined(SWITCH) || defined(OUNCE)
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

namespace LOCK {

namespace {

	double TruncDec(double value, double truncator) {
		size_t factor = pow(10, truncator);
		return trunc(value*factor) / factor;
	}

	bool NOINLINE isAddressValid(patch_addr_t address_in) {

		int64_t address = address_in;
		MemoryInfo memoryinfo = {0};
		uint32_t pageinfo = 0;

	#if defined(SWITCH) || defined(OUNCE)
		#define MIN_ASLR_ADDRESS 0x8000000
		#define MAX_ASLR_ADDRESS 0x7FFFFFFFFF
	#else
		#define MIN_ASLR_ADDRESS 0x200000
		#define MAX_ASLR_ADDRESS 0xFFFFFFFF
	#endif
		if (address < MIN_ASLR_ADDRESS || address > MAX_ASLR_ADDRESS) return false;

		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, address);
		if (R_FAILED(rc)) return false;
		if ((memoryinfo.perm & Perm_Rw) && ((address - memoryinfo.addr >= 0) && (address - memoryinfo.addr <= memoryinfo.size)))
			return true;
		return false;
	}

	inline FILE* fopen_sdcard(const char* path, const char* mode) {
		return SaltySDCore_fopen(path, mode);
	}

	inline size_t fread_sdcard(void* ptr, size_t size, size_t count, FILE* stream) {
		return SaltySDCore_fread(ptr, size, count, stream);
	}

	inline int fseek_sdcard(FILE* stream, int64_t offset, int origin) {
		return SaltySDCore_fseek(stream, offset, origin);
	}

	inline size_t ftell_sdcard(FILE* stream) {
		return SaltySDCore_ftell(stream); 
	}

	inline int fclose_sdcard(FILE* stream) {
		return SaltySDCore_fclose(stream); 
	}

	#define printf_sdcard SaltySDCore_printf

	inline Result memcpy_unsafe(uintptr_t to, uintptr_t from, size_t size) {
		return SaltySD_Memcpy(to, from, size);
	}

	template <typename E>
	constexpr auto enum_val(E e) {
		return static_cast<std::underlying_type_t<E>>(e);
	}

	template <typename T>
	void outWriteType (auto& in, auto& out) {
		out.template write<T>(in.template read<T>());
	}

	void outWriteVal (auto in, auto& out) {
		out.template write<decltype(in)>(in);
	}

	#define OUT_TYPE(T) outWriteType<T>(in, out)
	#define OUT_VAL(in) outWriteVal(in, out)
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

template <typename T>
void Patcher::Writer::write(T value) {
	memcpy(&m_data[m_offset], &value, sizeof(T));
	m_offset += sizeof(T);
}

void Patcher::Writer::copy(const uint8_t* src, size_t bytes) {
	// HOS requires from SIMD load/store instructions to have aligned pointers in A32 mode, so we must avoid using VSTR here
	memcpy(&m_data[m_offset], src, bytes);
	m_offset += bytes;
}

void Patcher::bindMainRegion(intptr_t main_start) {
	m_mappings.main_start = main_start;
#if defined(SWITCH) || defined(OUNCE)
	m_mappings.variables_start = (intptr_t)&variables_buffer[0];
	m_mappings.codeCave_start  = (intptr_t)&codeCave;
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
#if defined(SWITCH32) || defined(OUNCE32)
		int32_t temp_offset = cursor.read<int32_t>();
		address += temp_offset;
#else
		uint32_t temp_offset = cursor.read<uint32_t>();
		if (region > Region::Absolute && region < Region::Variables) {
			int32_t temp_offset_int = 0;
			memcpy(&temp_offset_int, &temp_offset, 4);
			address += (int64_t)temp_offset_int;
		}
		else address += (int64_t)temp_offset;
#endif
		if (i + 1 < offsets_count) {
			if (unsafe_address && !isAddressValid(*(patch_addr_t*)address)) return -2;
			address = *(patch_addr_t*)address;
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

#if defined(SWITCH) || defined(OUNCE)

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
			case CodeCaveAdjustmentType::Adrp_CodeCave:
			case CodeCaveAdjustmentType::Adrp_Variables:
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
		fread_sdcard(&OPCODE, 1, 1, file);
		printf_sdcard("LOCK: processes opcode: %d, offset: 0x%x\n", OPCODE, ftell_sdcard(file));
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
		case ValueType::F64:
		case ValueType::RefreshRate:
			tmp.d = value;
			break;
		case ValueType::F32:
			tmp.f = (float)value;	
			break;
		default:
			switch(enum_val(value_type) >> 4) {
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
	te_expr *n = te_compile(equation, vars, std::size(vars), 0);
	double evaluated_value = te_eval(n);
	te_free(n);
	return evaluated_value;
}

void Patcher::copyAddress(Cursor& in, Writer& out) const {
	if (m_gen >= MAX_SUPPORTED_GEN) OUT_TYPE(uint8_t);
	uint8_t address_count = in.read<uint8_t>();
	OUT_VAL(address_count);
	OUT_TYPE(Region);
	out.copy(in.take(sizeof(uint32_t)), address_count);
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
	Writer out(out_buffer, header_size);

	while (true) {
		const auto OPCODE = in.read<AllFpsOpcode>();
		switch (OPCODE) {
			case AllFpsOpcode::Write:
			case AllFpsOpcode::Eval_Write: {
				OUT_VAL(enum_val(OPCODE) & 0x7F);
				copyAddress(in, out);
				Result rc = copyValues(in, out, OPCODE == AllFpsOpcode::Eval_Write, FPS, refreshRate);
				if (R_FAILED(rc)) return rc;
				break;
			}
			case AllFpsOpcode::Compare:
			case AllFpsOpcode::Eval_Compare: {
				OUT_VAL(enum_val(OPCODE) & 0x7F);
				copyAddress(in, out);
				OUT_TYPE(CompareType);
				const auto value_type = in.read<ValueType>();
				OUT_VAL(value_type);
				const auto member_size = memberSize(value_type);
				out.copy(in.take(member_size), member_size);
				copyAddress(in, out);
				Result rc = copyValues(in, out, OPCODE == AllFpsOpcode::Eval_Compare, FPS, refreshRate);
				if (R_FAILED(rc)) return rc;
				break;
			}
			case AllFpsOpcode::Block:
				OUT_TYPE(AllFpsOpcode);
				OUT_TYPE(BlockOpcodeWhatType);
				break;
			case AllFpsOpcode::End:
				OUT_TYPE(AllFpsOpcode);
				return 0;
			default:
				return 0x2002;
		}
	}
}

Result Patcher::execWrite(Cursor& cursor) {
	patch_addr_t address = getAddress(cursor);
#if defined(SWITCH) || defined(OUNCE)
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
	memcpy((void*)address, cursor.take(array_size), array_size);
	return 0;
}

Result Patcher::execCompare(Cursor& cursor) {
	patch_addr_t address = getAddress(cursor);
#if defined(SWITCH) || defined(OUNCE)
	if (address < 0) return 0x6;
#endif

	const auto compare_type = cursor.read<CompareType>();
	ValueType value_type = cursor.read<ValueType>();
	bool passed = false;

	bool found = [&]<typename... M>(std::tuple<M...>) { 
		return (... || (value_type == M::val ? (compareValues(*(typename M::type*)(address), cursor.read<typename M::type>(), compare_type), true) : false)); 
	} (TypeMappings{});
	if (!found) return 0x8;

	address = getAddress(cursor);
#if defined(SWITCH) || defined(OUNCE)
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
	if (passed) memcpy((void*)address, source, array_size);
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
		m_compiled = (uint8_t*)malloc(m_compiledSize);
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
		printf_sdcard("LOCK: wrong header! Expected: 0x%lx, got: 0x%lx\n", header_size, tell);
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
