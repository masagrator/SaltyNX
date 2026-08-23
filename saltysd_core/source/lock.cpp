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

#if defined(SWITCH) || defined(OUNCE)
	using patch_addr_t = intptr_t;
#else
	using patch_addr_t = uintptr_t;
#endif

	double TruncDec(double value, double truncator) {
		size_t factor = pow(10, truncator);
		return trunc(value*factor) / factor;
	}

	bool NOINLINE isAddressValid(uintptr_t address_in) {

		int64_t address = address_in;
		MemoryInfo memoryinfo = {0};
		u32 pageinfo = 0;

	#if defined(SWITCH) || defined(OUNCE)
		if (address < 0x200000 || address > 0xFFFFFFFF) return false;
	#else
		if ((address < 0x8000000) || (address >= 0x8000000000)) return false;
	#endif

		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, address);
		if (R_FAILED(rc)) return false;
		if ((memoryinfo.perm & Perm_Rw) && ((address - memoryinfo.addr >= 0) && (address - memoryinfo.addr <= memoryinfo.size)))
			return true;
		return false;
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

	#define printf_sdcard SaltySDCore_printf

	inline Result memcpy_unsafe(uintptr_t to, uintptr_t from, size_t size) {
		return SaltySD_Memcpy(to, from, size);
	}
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
bool Patcher::compareValues(T value1, T value2, uint8_t compare_type) {
	switch (compare_type) {
		case 1: return (value1 >  value2);
		case 2: return (value1 >= value2);
		case 3: return (value1 <  value2);
		case 4: return (value1 <= value2);
		case 5: return (value1 == value2);
		case 6: return (value1 != value2);
	}
	return false;
}

intptr_t NOINLINE Patcher::getAddress(Cursor& cursor) const {
	bool unsafe_address = !m_unsafeCheck;
	if (m_gen == 4) unsafe_address = cursor.read<bool>();
	int8_t offsets_count = cursor.read<int8_t>();
	uint8_t region = cursor.read<uint8_t>();
	offsets_count -= 1;
	int64_t address = 0;
	switch (static_cast<Region>(region)) {
		case Region::Absolute:
			break;
		case Region::Main:
			address = m_mappings.main_start;
			break;
		case Region::Heap:
			address = m_mappings.heap_start;
			break;
		case Region::Alias:
			address = m_mappings.alias_start;
			break;
#if defined(SWITCH) || defined(OUNCE)
		case Region::Variables:
			address = m_mappings.variables_start;
			break;
		case Region::CodeCave:
			address = m_mappings.codeCave_start;
			break;
#endif
		default:
			return -1;
	}
	for (int i = 0; i < offsets_count; i++) {
#if defined(SWITCH32) || defined(OUNCE32)
		int32_t temp_offset = cursor.read<int32_t>();
		address += temp_offset;
#else
		uint32_t temp_offset = cursor.read<uint32_t>();
		if (region > 0 && region < 4) {
			int32_t temp_offset_int = 0;
			memcpy(&temp_offset_int, &temp_offset, 4);
			address += (int64_t)temp_offset_int;
		}
		else address += (int64_t)temp_offset;
#endif
		if (i + 1 < offsets_count) {
			if (unsafe_address && !isAddressValid(*(uintptr_t*)address)) return -2;
			address = *(uintptr_t*)address;
		}
	}
	return address;
}

bool Patcher::isBufferValid(const uint8_t* buffer, size_t filesize) {
	(void)filesize;

	const uint8_t MAGIC[4] = {'L', 'O', 'C', 'K'};
	if (memcmp(buffer, MAGIC, sizeof(MAGIC)) != 0)
		return false;
	m_gen = buffer[4];
	if (m_gen != 3 && m_gen != 4)
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
	uint32_t main_offset = 0;
	fread_sdcard(&main_offset, 4, 1, file);
	uint8_t value_type = 0;
	fread_sdcard(&value_type, 1, 1, file);
	uint8_t elements = 0;
	fread_sdcard(&elements, 1, 1, file);
	const auto member_size = memberSize(value_type);
	void* temp_buffer = calloc(elements, member_size);
	fread_sdcard(temp_buffer, member_size, elements, file);
	memcpy_unsafe(m_mappings.main_start + main_offset, (u64)temp_buffer, elements * member_size);
	free(temp_buffer);
	return 0;
}

#if defined(SWITCH) || defined(OUNCE)

Result Patcher::processVariables(FILE* file) {
	uint32_t main_offset = 0;
	fread_sdcard(&main_offset, 4, 1, file);
	uint8_t value_type = 0;
	fread_sdcard(&value_type, 1, 1, file);
	uint8_t elements = 0;
	fread_sdcard(&elements, 1, 1, file);
	const auto member_size = memberSize(value_type);
	void* temp_buffer = calloc(elements, member_size);
	fread_sdcard(temp_buffer, member_size, elements, file);
	memcpy_unsafe(m_mappings.variables_start + main_offset, (u64)temp_buffer, elements * member_size);
	free(temp_buffer);
	return 0;
}

Result Patcher::processCodeCave(FILE* file) {
	uint8_t address_region = 0;
	fread_sdcard(&address_region, 1, 1, file);
	uint32_t main_offset = 0;
	fread_sdcard(&main_offset, 4, 1, file);
	uint8_t value_type = 0;
	fread_sdcard(&value_type, 1, 1, file);
	uint8_t elements = 0;
	fread_sdcard(&elements, 1, 1, file);

	struct codeCaveData {
		uint8_t adjustment_type;
		uint32_t instruction;
	} PACKED;
	static_assert(sizeof(codeCaveData) == 5);

	codeCaveData* temp_buffer = (codeCaveData*)calloc(elements, sizeof(codeCaveData));
	uint32_t* output = 0;
	if (address_region == (uint8_t)Region::CodeCave) output = (uint32_t*)(m_mappings.codeCave_start + main_offset);
	else if (address_region == (uint8_t)Region::Main) output = (uint32_t*)(m_mappings.main_start + main_offset);
	else return 0x321;
	fread_sdcard(temp_buffer, sizeof(codeCaveData), elements, file);
	for (size_t i = 0; i < elements; i++) {
		switch (temp_buffer[i].adjustment_type) {
			case 0:
				memcpy_unsafe((u64)&output[i], (u64)&temp_buffer[i].instruction, 4);
				break;
			case 1: {
				struct {
					signed int imm: 26;
					unsigned int opcode: 6;
				} Branch;
				static_assert(sizeof(Branch) == 4);
				memcpy(&Branch, &temp_buffer[i].instruction, 4);
				intptr_t current_address = (intptr_t)&output[i];
				if (Branch.imm == -1) {
					intptr_t jump_address = (intptr_t)&Utils::_convertToTimeSpan;
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				else if (Branch.imm == -2) {
					intptr_t jump_address = (intptr_t)&nn::SetUserInactivityDetectionTimeExtended;
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				else if (Branch.imm <= -64) {
					intptr_t jump_address = (m_mappings.codeCave_start - 0x100) + (((int64_t)(Branch.imm)*4) * -1);
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				else if (address_region == (uint8_t)Region::CodeCave) {
					intptr_t jump_address = (intptr_t)(m_mappings.main_start + ((int64_t)(Branch.imm)*4 + (main_offset + (i*4))));
					current_address = (intptr_t)&output[i];
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
				}
				memcpy_unsafe((u64)&output[i], (u64)&Branch, 4);
				break;
			}
			case 2:
			case 3:
			case 4: {
				struct {
					unsigned int reg: 5;
					signed int immhi: 19;
					unsigned int reserved: 5;
					unsigned int immlo: 2;
					bool op: 1;
				} ADRP;
				static_assert(sizeof(ADRP) == 4);
				memcpy(&ADRP, &temp_buffer[i].instruction, 4);
				intptr_t current_address = (intptr_t)(&output[i]) & ~0xFFF;
				intptr_t jump_address = 0;
				ptrdiff_t offset = 0;
				if (temp_buffer[i].adjustment_type == 2) {
					jump_address = (intptr_t)m_mappings.codeCave_start;
					offset = jump_address - current_address;
				}
				else if (temp_buffer[i].adjustment_type == 3) {
					jump_address = (intptr_t)m_mappings.variables_start;
					offset = jump_address - current_address;
				}
				else if (temp_buffer[i].adjustment_type == 4) {
					jump_address = (intptr_t)(((uintptr_t)ADRP.immlo << 12) + ((uintptr_t)ADRP.immhi << 14));
					offset = jump_address + (m_mappings.main_start - m_mappings.codeCave_start);
				}
				ADRP.immlo = (offset % 0x4000) >> 12;
				ADRP.immhi = (offset >> 14);
				memcpy_unsafe((u64)&output[i], (u64)&ADRP, 4);
				break;
			}
			case 5: {
				struct {
					signed int imm: 26;
					unsigned int opcode: 6;
				} Branch;
				static_assert(sizeof(Branch) == 4);
				memcpy(&Branch, &temp_buffer[i].instruction, 4);
				intptr_t current_address = (intptr_t)&output[i];
				intptr_t jump_address = (intptr_t)(m_mappings.main_start + ((int64_t)(Branch.imm)*4) + (i*4));
				ptrdiff_t offset = jump_address - current_address;
				Branch.imm = offset / 4;
				memcpy_unsafe((u64)&output[i], (u64)&Branch, 4);
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

	int8_t OPCODE = 0;
	Result rc = 0;
	while (true) {
		fread_sdcard(&OPCODE, 1, 1, file);
		printf_sdcard("processes opcode: %d, offset: 0x%x\n", OPCODE, ftell_sdcard(file));
		switch (OPCODE) {
			case 1: {rc = processBytes(file); break;}
#if defined(SWITCH) || defined(OUNCE)
			case 2: {rc = processVariables(file); break;}
			case 3: {rc = processCodeCave(file); break;}
#endif
			case -1: {m_masterWriteApplied = true; return 0;}
			default: return 0x355;
		}
		if (R_FAILED(rc)) return rc;
	}
}

Result Patcher::writeExprTo(double value, Writer& out, uint8_t value_type) {
	uint8_t size = memberSize(value_type);
	union {
		uint64_t u;
		int64_t i;
		double d;
		float f;
	} tmp;

	switch (value_type >> 4) {
		case 0:
			tmp.u = (uint64_t)value;
			break;
		case 1:
			tmp.i = (int64_t)value;
			break;
		case 2:
			if (size == 4) {
				tmp.f = (float)value;
				break;
			}
			//Fallthrough
		case 3:
			tmp.d = value;
			break;
		default:
			return 4;
	}

	out.copy(reinterpret_cast<const uint8_t*>(&tmp), size);
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
	te_variable vars[] = {
		{"TruncDec", (const void*)TruncDec, TE_FUNCTION2},
		{"FPS_TARGET", &FPS_TARGET, TE_VARIABLE},
		{"FPS_LOCK_TARGET", &FPS_LOCK_TARGET, TE_VARIABLE},
		{"FRAMETIME_TARGET", &FRAMETIME_TARGET, TE_VARIABLE},
		{"VSYNC_TARGET", &VSYNC_TARGET, TE_VARIABLE},
		{"INTERVAL_TARGET", &INTERVAL_TARGET, TE_VARIABLE},
		{"REFRESH_RATE", &displaySync, TE_VARIABLE}
	};
	te_expr *n = te_compile(equation, vars, std::size(vars), 0);
	double evaluated_value = te_eval(n);
	te_free(n);
	return evaluated_value;
}

void Patcher::copyAddress(Cursor& in, Writer& out) const {
	if (m_gen == 4) out.write<uint8_t>(in.read<uint8_t>());
	uint8_t address_count = in.read<uint8_t>();
	out.write<uint8_t>(address_count);
	out.write<uint8_t>(in.read<uint8_t>()); // address region
	for (size_t i = 1; i < address_count; i++) {
		out.write<uint32_t>(in.read<uint32_t>());
	}
}

Result Patcher::copyValues(Cursor& in, Writer& out, bool evaluate,
                           uint8_t FPS, uint8_t refreshRate) const {
	uint8_t value_type = in.read<uint8_t>();
	out.write<uint8_t>(value_type);
	uint8_t value_count = in.read<uint8_t>();
	out.write<uint8_t>(value_count);

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

Result NOINLINE Patcher::convertPatchToFPSTarget(uint8_t* out_buffer, const uint8_t* in_buffer,
                                                 uint8_t FPS, uint8_t refreshRate) {
	uint32_t header_size = 0;
	memcpy(&header_size, &in_buffer[8], 4);
	memcpy(out_buffer, in_buffer, header_size);

	Cursor in(in_buffer, header_size);
	Writer out(out_buffer, header_size);

	while (true) {
		const auto OPCODE = in.read<uint8_t>();
		switch (OPCODE) {
			// write, plain or with expressions to evaluate (0x80 flag)
			case 1:
			case 0x81: {
				out.write<uint8_t>(OPCODE & 0x80);
				copyAddress(in, out);
				Result rc = copyValues(in, out, (OPCODE & 0x80) == 0x80, FPS, refreshRate);
				if (R_FAILED(rc)) return rc;
				break;
			}
			// compare, plain or with expressions to evaluate (0x80 flag)
			case 2:
			case 0x82: {
				out.write<uint8_t>(OPCODE & 0x80);
				copyAddress(in, out);
				out.write<uint8_t>(in.read<uint8_t>()); // compare_type
				const auto value_type = in.read<uint8_t>();
				out.write<uint8_t>(value_type);
				const auto member_size = memberSize(value_type);
				out.copy(in.take(member_size), member_size);
				copyAddress(in, out);
				Result rc = copyValues(in, out, (OPCODE & 0x80) == 0x80, FPS, refreshRate);
				if (R_FAILED(rc)) return rc;
				break;
			}
			// block
			case 3:
				out.write<uint8_t>(3);
				out.write<uint8_t>(in.read<uint8_t>());
				break;
			// end of execution
			case 255:
				out.write<uint8_t>(255);
				return 0;
			default:
				return 0x2002;
		}
	}
}

Result Patcher::execWrite(Cursor& cursor) {
	patch_addr_t address = getAddress(cursor);
#if defined(SWITCH) || defined(OUNCE)
	if (address < 0) return 6;
#endif

	/* value_type:
		1		=	uint8
		2		=	uint16
		4		=	uint32
		8		=	uint64
		0x11	=	int8
		0x12	=	int16
		0x14	=	int32
		0x18	=	int64
		0x24	=	float
		0x28	=	double
	*/
	const auto value_type = cursor.read<uint8_t>();
	const auto loops = cursor.read<uint8_t>();

	if (value_type == (uint8_t)ValueType::RefreshRate) {
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
	if (address < 0) return 6;
#endif

	/* compare_type:
		1	=	>
		2	=	>=
		3	=	<
		4	=	<=
		5	=	==
		6	=	!=
	*/
	const auto compare_type = cursor.read<uint8_t>();
	uint8_t value_type = cursor.read<uint8_t>();
	bool passed = false;

	auto doCompare = [&]<typename T>(std::in_place_type_t<T>) {
		passed = compareValues(*reinterpret_cast<const T*>(address), cursor.read<T>(), compare_type);
	};

	switch (value_type) {
		case 1:    doCompare(std::in_place_type<uint8_t>);  break;
		case 2:    doCompare(std::in_place_type<uint16_t>); break;
		case 4:    doCompare(std::in_place_type<uint32_t>); break;
		case 8:    doCompare(std::in_place_type<uint64_t>); break;
		case 0x11: doCompare(std::in_place_type<int8_t>);   break;
		case 0x12: doCompare(std::in_place_type<int16_t>);  break;
		case 0x14: doCompare(std::in_place_type<int32_t>);  break;
		case 0x18: doCompare(std::in_place_type<int64_t>);  break;
		case 0x24: doCompare(std::in_place_type<float>);    break;
		case 0x28: doCompare(std::in_place_type<double>);   break;
		default:
			return 8;
	}

	address = getAddress(cursor);
#if defined(SWITCH) || defined(OUNCE)
	if (address < 0) return 6;
#endif
	value_type = cursor.read<uint8_t>();
	const auto loops = cursor.read<uint8_t>();

	if (value_type == (uint8_t)ValueType::RefreshRate) {
		for (uint8_t i = 0; i < loops; i++) {
			const auto valueDouble = cursor.read<uint64_t>();
			if (passed) memcpy(&m_overwriteRefreshRate, &valueDouble, sizeof(valueDouble));
		}
		return 0;
	}

	if (!address) return 0x3007;
	const auto member_size = memberSize(value_type);
	if (!validMemberSize(member_size))
		return 9;
	const auto array_size = member_size * loops;
	const auto source = cursor.take(array_size);
	if (passed) memcpy((void*)address, source, array_size);
	return 0;
}

Result Patcher::execBlock(Cursor& cursor) {
	switch (cursor.read<uint8_t>()) {
		case 1:
			m_blockDelayFPS = true;
			return 0;
		default:
			return 7;
	}
}

Result Patcher::applyPatch(const uint8_t* buffer, uint8_t FPS, uint8_t refreshRate) {
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
		if (R_FAILED(convertPatchToFPSTarget(m_compiled, buffer, FPS, refreshRate))) {
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
		/* OPCODE:
			0	=	err
			1	=	write
			2	=	compare
			3	=	block
			-1	=	endExecution
		*/
		const auto OPCODE = cursor.read<int8_t>();
		Result rc = 0;
		switch (OPCODE) {
			case 1:  rc = execWrite(cursor);   break;
			case 2:  rc = execCompare(cursor); break;
			case 3:  rc = execBlock(cursor);   break;
			case -1: return 0;
			default: return 255;
		}
		if (R_FAILED(rc)) return rc;
	}
}

}
