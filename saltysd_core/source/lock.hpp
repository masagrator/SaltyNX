#pragma once
#define NOINLINE __attribute__ ((noinline))
#include "tinyexpr/tinyexpr.h"
#include <array>

#if defined(SWITCH) || defined(OUNCE)
//We need to define something in that section and reference its pointer to not get whole section discarded by garbage collector
void __attribute__ ((section(".codecave"))) codeCave() {}

size_t codeCave_buffer_reserved = 0;

alignas(0x1000) uint8_t variables_buffer[0x1000];

size_t variables_buffer_reserved = 0;

namespace Utils {
	uint64_t _convertToTimeSpan(uint64_t tick);
}

namespace nn {
	Result SetUserInactivityDetectionTimeExtended(bool isTrue);
}

#endif

namespace LOCK {

	uint32_t offset = 0;
	bool blockDelayFPS = false;
	uint8_t gen = 3;
	bool MasterWriteApplied = false;
	double overwriteRefreshRate = 0;
	size_t DockedRefreshRateDelay = 4000000000;
	uint8_t masterWrite = 0;
	uint32_t compiledSize = 0;

	struct {
		intptr_t main_start;
		uintptr_t alias_start;
		uintptr_t heap_start;
		intptr_t variables_start;
		intptr_t codeCave_start;
	} mappings;

	template <typename T>
	bool compareValues(T value1, T value2, uint8_t compare_type) { // 1 - >, 2 - >=, 3 - <, 4 - <=, 5 - ==, 6 - !=
		switch(compare_type) {
			case 1:
				return (value1 > value2);
			case 2:
				return (value1 >= value2);
			case 3:
				return (value1 < value2);
			case 4:
				return (value1 <= value2);
			case 5:
				return (value1 == value2);
			case 6:
				return (value1 != value2);
		}
		return false;
	}

	template <typename T>
	T read(uint8_t* buffer) {
		T ret;
		memcpy(&ret, &buffer[offset], sizeof(T));
		offset += sizeof(T);
		return ret;
	}

	uintptr_t getBufferOffset(uint8_t* buffer, ptrdiff_t offset_shift) {
		uintptr_t buffer_ptr = (uintptr_t)&buffer[offset];
		offset += offset_shift;
		return buffer_ptr;
	}

	template <typename T>
	void writeValue(T value, uintptr_t address) {
		memcpy((void*)address, &value, sizeof(T));
	}

	bool unsafeCheck = false;

	bool NOINLINE isAddressValid(uintptr_t address_in) {

		int64_t address = address_in;
		MemoryInfo memoryinfo = {0};
		u32 pageinfo = 0;

		#if defined(SWITCH32) || defined(OUNCE32)
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

	intptr_t NOINLINE getAddress(uint8_t* buffer) {
		bool unsafe_address = !unsafeCheck;
		if (gen == 4) unsafe_address = read<bool>(buffer);
		int8_t offsets_count = read<int8_t>(buffer);
		uint8_t region = read<uint8_t>(buffer);
		offsets_count -= 1;
		int64_t address = 0;
		switch(region) {
			case 0:
				break;
			case 1:
				address = mappings.main_start;
				break;
			case 2:
				address = mappings.heap_start;
				break;
			case 3:
				address = mappings.alias_start;
				break;
			#if defined(SWITCH) || defined(OUNCE)
			case 4:
				address = mappings.variables_start;
				break;
			case 5:
				address = mappings.codeCave_start;
				break;
			#endif
			default:
				return -1;
		}
		for (int i = 0; i < offsets_count; i++) {
			#if defined(SWITCH32) || defined(OUNCE32)
			int32_t temp_offset = read<int32_t>(buffer);
			address += temp_offset;
			if (i+1 < offsets_count) {
				if (unsafe_address && !isAddressValid(*(uintptr_t*)address)) return -2;
				address = *(uintptr_t*)address;
			}
			#else
			uint32_t temp_offset = read<uint32_t>(buffer);
			if (region > 0 && region < 4) {
				int32_t temp_offset_int = 0;
				memcpy(&temp_offset_int, &temp_offset, 4);
				address += (int64_t)temp_offset_int;
			}
			else address += (int64_t)temp_offset;
			if (i+1 < offsets_count) {
				if (unsafe_address && !isAddressValid(*(uintptr_t*)address)) return -2;
				address = *(uintptr_t*)address;
			}
			#endif
		}
		return address;
	}


///2. File format and reading

	bool isValid(uint8_t* buffer, size_t filesize) {
		uint8_t MAGIC[4] = {'L', 'O', 'C', 'K'};
		if (*(uint32_t*)buffer != *(uint32_t*)&MAGIC)
			return false;
		gen = buffer[4];
		if (gen != 3 && gen != 4)
			return false;
		masterWrite = buffer[5];
		if (masterWrite > 1)
			return false;
		unsafeCheck = (bool)buffer[7];
		uint8_t start_offset = 0xC;
		if (masterWrite) start_offset += 4;
		if (*(uint32_t*)(&(buffer[8])) != start_offset)
			return false;
		compiledSize = (uint32_t)buffer[6] * buffer[6];
		return true;

	}

	Result processBytes(FILE* file) {
		uint32_t main_offset = 0;
		SaltySDCore_fread(&main_offset, 4, 1, file);
		uint8_t value_type = 0;
		SaltySDCore_fread(&value_type, 1, 1, file);
		uint8_t elements = 0;
		SaltySDCore_fread(&elements, 1, 1, file);
		const uint8_t member_size = value_type % 0x10;
		void* temp_buffer = calloc(elements, member_size);
		SaltySDCore_fread(temp_buffer, member_size, elements, file);
		SaltySD_Memcpy(LOCK::mappings.main_start + main_offset, (u64)temp_buffer, elements * member_size);
		free(temp_buffer);
		return 0;
	}

#if defined(SWITCH) || defined(OUNCE)

	Result processVariables(FILE* file) {
		uint32_t main_offset = 0;
		SaltySDCore_fread(&main_offset, 4, 1, file);
		uint8_t value_type = 0;
		SaltySDCore_fread(&value_type, 1, 1, file);
		uint8_t elements = 0;
		SaltySDCore_fread(&elements, 1, 1, file);
		const uint8_t member_size = value_type % 0x10;
		void* temp_buffer = calloc(elements, member_size);
		SaltySDCore_fread(temp_buffer, member_size, elements, file);
		SaltySD_Memcpy(LOCK::mappings.variables_start + main_offset, (u64)temp_buffer, elements * member_size);
		free(temp_buffer);
		return 0;
	}

	Result processCodeCave(FILE* file) {
		uint8_t address_region = 0;
		SaltySDCore_fread(&address_region, 1, 1, file);
		uint32_t main_offset = 0;
		SaltySDCore_fread(&main_offset, 4, 1, file);
		uint8_t value_type = 0;
		SaltySDCore_fread(&value_type, 1, 1, file);
		uint8_t elements = 0;
		SaltySDCore_fread(&elements, 1, 1, file);
		struct codeCave {
			uint8_t adjustment_type;
			uint32_t instruction;
		} PACKED;
		static_assert(sizeof(codeCave) == 5);
		codeCave* temp_buffer = (codeCave*)calloc(elements, sizeof(codeCave));
		uint32_t* output = 0;
		if (address_region == 5) output = (uint32_t*)(LOCK::mappings.codeCave_start + main_offset);
		else if (address_region == 1) output = (uint32_t*)(LOCK::mappings.main_start + main_offset);
		else return 0x321;
		SaltySDCore_fread(temp_buffer, sizeof(codeCave), elements, file);
		for (size_t i = 0; i < elements; i++) {
			switch(temp_buffer[i].adjustment_type) {
				case 0:
					SaltySD_Memcpy((u64)&output[i], (u64)&temp_buffer[i].instruction, 4);
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
						intptr_t jump_address = (LOCK::mappings.codeCave_start - 0x100) + (((int64_t)(Branch.imm)*4) * -1);
						ptrdiff_t offset = jump_address - current_address;
						Branch.imm = offset / 4;
					}
					else if (address_region == 5) {
						intptr_t jump_address = (intptr_t)(LOCK::mappings.main_start + ((int64_t)(Branch.imm)*4 + (main_offset + (i*4))));
						current_address = (intptr_t)&output[i];
						ptrdiff_t offset = jump_address - current_address;
						Branch.imm = offset / 4;
					}
					SaltySD_Memcpy((u64)&output[i], (u64)&Branch, 4);
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
						jump_address = (intptr_t)LOCK::mappings.codeCave_start;
						offset = jump_address - current_address;
					}
					else if (temp_buffer[i].adjustment_type == 3) {
						jump_address = (intptr_t)LOCK::mappings.variables_start;
						offset = jump_address - current_address;
					}
					else if (temp_buffer[i].adjustment_type == 4) {
						jump_address = (intptr_t)(((uintptr_t)ADRP.immlo << 12) + ((uintptr_t)ADRP.immhi << 14));
						offset = jump_address + (LOCK::mappings.main_start - LOCK::mappings.codeCave_start);
					}
					ADRP.immlo = (offset % 0x4000) >> 12;
					ADRP.immhi = (offset >> 14);
					uint32_t inst = 0;
					memcpy(&inst, &ADRP, 4);
					SaltySD_Memcpy((u64)&output[i], (u64)&ADRP, 4);
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
					intptr_t jump_address = (intptr_t)(LOCK::mappings.main_start + ((int64_t)(Branch.imm)*4) + (i*4));
					ptrdiff_t offset = jump_address - current_address;
					Branch.imm = offset / 4;
					SaltySD_Memcpy((u64)&output[i], (u64)&Branch, 4);
					break;					
				}
				default:
					return 0x345;
			}
		}
		free(temp_buffer);
		return 0; //TO DO
	}

#endif
	Result applyMasterWrite(FILE* file, size_t master_offset) {
		uint32_t offset_impl = 0;

		SaltySDCore_fseek(file, master_offset, 0);
		SaltySDCore_fread(&offset_impl, 4, 1, file);
		SaltySDCore_fseek(file, offset_impl, 0);
		if (SaltySDCore_ftell(file) != offset_impl)
			return 0x312;
		
		int8_t OPCODE = 0;
		Result rc = 0;
		while(true) {
			SaltySDCore_fread(&OPCODE, 1, 1, file);
			SaltySDCore_printf("processes opcode: %d, offset: 0x%x\n", OPCODE, ftell(file));
			switch(OPCODE) {
				case 1: {rc = processBytes(file); break;}
				#if defined(SWITCH) || defined(OUNCE)
				case 2: {rc = processVariables(file); break;}
				case 3: {rc = processCodeCave(file); break;}
				#endif
				case -1: {MasterWriteApplied = true; return 0;}
				default: return 0x355;
			}
			if (R_FAILED(rc)) return rc;
		}
	}

	Result writeExprTo(double value, uint8_t* buffer, uint16_t* offset_impl, uint8_t value_type) {
		uint8_t size = value_type % 0x10;
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

		if (size == 1) memcpy(&buffer[*offset_impl], &tmp, 1);
		else if (size == 2) memcpy(&buffer[*offset_impl], &tmp, 2);
		else if (size == 4) memcpy(&buffer[*offset_impl], &tmp, 4);
		else memcpy(&buffer[*offset_impl], &tmp, 8); //HOS requires from SIMD load/store instructions to have aligned pointers in A32 mode, so we must avoid using VSTR here
		*offset_impl += size;
		return 0;
	}
	
	double TruncDec(double value, double truncator) {
		uint64_t factor = pow(10, truncator);
		return trunc(value*factor) / factor;
	}

	double NOINLINE evaluateExpression(const char* equation, double fps_target, double displaySync) {
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
			{"INTERVAL_TARGET", &INTERVAL_TARGET, TE_VARIABLE}
		};
		te_expr *n = te_compile(equation, vars, std::size(vars), 0);
		double evaluated_value = te_eval(n);
		te_free(n);
		return evaluated_value;
	}

	Result NOINLINE convertPatchToFPSTarget(uint8_t* out_buffer, uint8_t* in_buffer, uint8_t FPS, uint8_t refreshRate) {
		uint32_t header_size = 0;
		memcpy(&header_size, &in_buffer[8], 4);
		memcpy(out_buffer, in_buffer, header_size);
		offset = header_size;
		uint16_t temp_offset = header_size;
		while(true) {
			uint8_t OPCODE = read<uint8_t>(in_buffer);
			if (OPCODE == 1 || OPCODE == 0x81) {
				bool evaluate = false;
				if (OPCODE == 0x81) {
					evaluate = true;
					OPCODE = 1;
				}
				out_buffer[temp_offset++] = OPCODE;
				if (gen == 4) out_buffer[temp_offset++] = read<uint8_t>(in_buffer);
				uint8_t address_count = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = address_count;
				out_buffer[temp_offset++] = read<uint8_t>(in_buffer);
				for (size_t i = 1; i < address_count; i++) {
					*(uint32_t*)&out_buffer[temp_offset] = read<uint32_t>(in_buffer);
					temp_offset += 4;
				}
				uint8_t value_type = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = value_type;
				uint8_t value_count = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = value_count;
				if (!evaluate) {
					uint8_t member_size = value_type % 0x10;
					size_t array_size = member_size * value_count;
					memcpy(&out_buffer[temp_offset], &in_buffer[offset], member_size * value_count);
					offset += array_size;
					temp_offset += array_size;
				}
				else for (size_t i = 0; i < value_count; i++) {
					double evaluated_value = evaluateExpression((const char*)&in_buffer[offset], (double)FPS, (double)refreshRate);
					offset += strlen((const char*)&in_buffer[offset]) + 1;
					writeExprTo(evaluated_value, out_buffer, &temp_offset, value_type);
				}
			}
			else if (OPCODE == 2 || OPCODE == 0x82) {
				bool evaluate = false;
				if (OPCODE == 0x82) {
					evaluate = true;
					OPCODE = 2;
				}
				out_buffer[temp_offset++] = OPCODE;
				if (gen == 4) out_buffer[temp_offset++] = read<uint8_t>(in_buffer);
				uint8_t address_count = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = address_count;
				out_buffer[temp_offset++] = read<uint8_t>(in_buffer); //compare address region
				for (size_t i = 1; i < address_count; i++) {
					*(uint32_t*)&out_buffer[temp_offset] = read<uint32_t>(in_buffer);
					temp_offset += 4;
				}
				out_buffer[temp_offset++] = read<uint8_t>(in_buffer); //compare_type
				uint8_t value_type = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = value_type;
				uint8_t member_size = value_type % 0x10;
				if (member_size == 1) memcpy(&out_buffer[temp_offset], &in_buffer[offset], 1);
				else if (member_size == 2) memcpy(&out_buffer[temp_offset], &in_buffer[offset], 2);
				else if (member_size == 4) memcpy(&out_buffer[temp_offset], &in_buffer[offset], 4);
				else memcpy(&out_buffer[temp_offset], &in_buffer[offset], 8);
				temp_offset += member_size;
				offset += member_size;
				if (gen == 4) out_buffer[temp_offset++] = read<uint8_t>(in_buffer);
				address_count = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = address_count;
				out_buffer[temp_offset++] = read<uint8_t>(in_buffer); //address region
				for (size_t i = 1; i < address_count; i++) {
					*(uint32_t*)&out_buffer[temp_offset] = read<uint32_t>(in_buffer);
					temp_offset += 4;
				}
				value_type = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = value_type;
				uint8_t value_count = read<uint8_t>(in_buffer);
				out_buffer[temp_offset++] = value_count;
				if (!evaluate) {
					member_size = value_type % 0x10;
					size_t array_size = member_size * value_count;
					memcpy(&out_buffer[temp_offset], &in_buffer[offset], member_size * value_count);
					offset += array_size;
					temp_offset += array_size;
				}
				else for (size_t i = 0; i < value_count; i++) {
					double evaluated_value = evaluateExpression((const char*)&in_buffer[offset], (double)FPS, (double)refreshRate);
					offset += strlen((const char*)&in_buffer[offset]) + 1;
					writeExprTo(evaluated_value, out_buffer, &temp_offset, value_type);
				}
			}
			else if (OPCODE == 3) {
				out_buffer[temp_offset++] = OPCODE;
				out_buffer[temp_offset++] = read<uint8_t>(in_buffer);
			}
			else if (OPCODE == 255) {
				out_buffer[temp_offset++] = OPCODE;
				break;
			}
			else return 0x2002;
		}
		return 0;
	}

	Result applyPatch(uint8_t* buffer, uint8_t FPS, uint8_t refreshRate = 60) {
		overwriteRefreshRate = 0;
		blockDelayFPS = false;
		static uint8_t* new_buffer = 0;
		static uint8_t lastFPS = 0;
		static uint8_t lastRefreshRate = 0;
		if (!refreshRate) refreshRate = 60;

		if ((lastFPS != FPS) || (lastRefreshRate != refreshRate)) {
			if (new_buffer != 0) {
				free(new_buffer);
			}
			new_buffer = (uint8_t*)malloc(compiledSize);
			if (!new_buffer)
				return 0x3004;
			if (R_FAILED(convertPatchToFPSTarget(new_buffer, buffer, FPS, refreshRate))) {
				lastFPS = 0;
				return 0x3002;
			}
			lastFPS = FPS;
			lastRefreshRate = refreshRate;
		}
		if (!new_buffer) {
			return 0x3003;
		}
		buffer = new_buffer;
		offset = *(uint32_t*)(&buffer[0x8]);
		while(true) {
			/* OPCODE:
				0	=	err
				1	=	write
				2	=	compare
				3	=	block
				-1	=	endExecution
			*/
			int8_t OPCODE = read<int8_t>(buffer);
			if (OPCODE == 1) {
				#if defined(SWITCH32) || defined(OUNCE32)
				uintptr_t address = getAddress(buffer);
				#else
				int64_t address = getAddress(buffer);
				if (address < 0) 
					return 6;
				#endif
				/* value_type:
					1		=	uint8
					2		=	uin16
					4		=	uint32
					8		=	uint64
					0x11	=	int8
					0x12	=	in16
					0x14	=	int32
					0x18	=	int64
					0x24	=	float
					0x28	=	double
				*/
				uint8_t value_type = read<uint8_t>(buffer);
				uint8_t loops = read<uint8_t>(buffer);
				if (value_type == 0x38) for (uint8_t i = 0; i < loops; i++) {
					overwriteRefreshRate = read<double>(buffer);
				}
				else {
					if (!address) return 0x3007;
					uint8_t member_size = value_type % 0x10;
					if (member_size > 8 || member_size == 0 || (member_size & (member_size - 1))) 
						return 3;
					size_t array_size = member_size * loops;
					uintptr_t buffer_ptr = getBufferOffset(buffer, array_size);
					memcpy((void*)address, (void*)buffer_ptr, array_size);
					address += array_size;
				}
			}
			else if (OPCODE == 2) {
				#if defined(SWITCH32) || defined(OUNCE32)
				uintptr_t address = getAddress(buffer);
				#else
				int64_t address = getAddress(buffer);
				if (address < 0) 
					return 6;
				#endif
				/* compare_type:
					1	=	>
					2	=	>=
					3	=	<
					4	=	<=
					5	=	==
					6	=	!=
				*/
				uint8_t compare_type = read<uint8_t>(buffer);
				uint8_t value_type = read<uint8_t>(buffer);
				bool passed = false;

				auto doCompare = [&]<typename T>(std::in_place_type_t<T>) {
					passed = compareValues(*reinterpret_cast<const T*>(address), read<T>(buffer), compare_type);
				};

				switch(value_type) {
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

				address = getAddress(buffer);
				if (address < 0) 
					return 6;
				value_type = read<uint8_t>(buffer);
				uint8_t loops = read<uint8_t>(buffer);
				if (value_type == 0x38) {
					for (uint8_t i = 0; i < loops; i++) {
						uint64_t valueDouble = read<uint64_t>(buffer);
						if (passed) writeValue(valueDouble, (uint64_t)&overwriteRefreshRate);
					}
				}
				else {
					if (!address) return 0x3007;
					uint8_t member_size = value_type % 0x10;
					if (member_size > 8 || member_size == 0 || (member_size & (member_size - 1))) 
						return 9;
					size_t array_size = member_size * loops;
					uintptr_t buffer_ptr = getBufferOffset(buffer, array_size);
					if (passed) memcpy((void*)address, (void*)buffer_ptr, array_size);
					address += array_size;
				}
			}
			else if (OPCODE == 3) {
				switch(read<uint8_t>(buffer)) {
					case 1:
						blockDelayFPS = true;
						break;
					default: 
						return 7;
				}
			}
			else if (OPCODE == -1) {
				return 0;
			}
			else return 255;
		}
	}
}
