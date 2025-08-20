#pragma once
#define NOINLINE __attribute__ ((noinline))
#include "tinyexpr/tinyexpr.h"
#include <array>

void codeCave(void) __attribute__ ((section(".codecave")));

void codeCave() {
	return;
}

size_t codeCave_buffer_reserved = 0;

alignas(0x1000) uint8_t variables_buffer[0x1000];

size_t variables_buffer_reserved = 0;

namespace Utils {
	uint64_t _convertToTimeSpan(uint64_t tick);
}

namespace nn {
	Result SetUserInactivityDetectionTimeExtended(bool isTrue);
}

namespace LOCK {

	uint32_t offset = 0;
	bool blockDelayFPS = false;
	uint8_t gen = 3;
	bool MasterWriteApplied = false;
	double overwriteRefreshRate = 0;
	uint64_t DockedRefreshRateDelay = 4000000000;
	uint8_t masterWrite = 0;
	uint32_t compiledSize = 0;

	struct {
		int64_t main_start;
		uint64_t alias_start;
		uint64_t heap_start;
		int64_t variables_start;
		int64_t codeCave_start;
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

	uint8_t read8(uint8_t* buffer) {
		uint8_t ret = buffer[offset];
		offset += sizeof(uint8_t);
		return ret;
	}

	uint16_t read16(uint8_t* buffer) {
		uint16_t ret = *(uint16_t*)(&buffer[offset]);
		offset += sizeof(uint16_t);
		return ret;
	}

	uint32_t read32(uint8_t* buffer) {
		uint32_t ret = *(uint32_t*)(&buffer[offset]);
		offset += sizeof(uint32_t);
		return ret;
	}

	uint64_t read64(uint8_t* buffer) {
		uint64_t ret = *(uint64_t*)(&buffer[offset]);
		offset += sizeof(uint64_t);
		return ret;
	}

	float readFloat(uint8_t* buffer) {
		float ret = *(float*)(&buffer[offset]);
		offset += sizeof(float);
		return ret;
	}

	double readDouble(uint8_t* buffer) {
		double ret = *(double*)(&buffer[offset]);
		offset += sizeof(double);
		return ret;
	}

	template <typename T>
	void writeValue(T value, int64_t address) {
		if (*(T*)address != value)
			*(T*)address = value;
	}

	bool unsafeCheck = false;

	bool NOINLINE isAddressValid(int64_t address) {
		MemoryInfo memoryinfo = {0};
		u32 pageinfo = 0;

		if (unsafeCheck) return true;

		if ((address < 0) || (address >= 0x8000000000)) return false;

		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, address);
		if (R_FAILED(rc)) return false;
		if ((memoryinfo.perm & Perm_Rw) && ((address - memoryinfo.addr >= 0) && (address - memoryinfo.addr <= memoryinfo.size)))
			return true;
		return false;
	}

	int64_t NOINLINE getAddress(uint8_t* buffer, uint8_t offsets_count) {
		uint8_t region = read8(buffer);
		offsets_count -= 1;
		int64_t address = 0;
		switch(region) {
			case 1:
				address = mappings.main_start;
				break;
			case 2:
				address = mappings.heap_start;
				break;
			case 3:
				address = mappings.alias_start;
				break;
			case 4:
				address = mappings.variables_start;
				break;
			case 5:
				address = mappings.codeCave_start;
				break;
			default:
				return -1;
		}
		for (int i = 0; i < offsets_count; i++) {
			uint32_t temp_offset = read32(buffer);
			address += (int64_t)temp_offset;
			if (i+1 < offsets_count) {
				if (!isAddressValid(*(int64_t*)address)) return -2;
				address = *(uint64_t*)address;
			}
		}
		return address;
	}


///2. File format and reading

	bool isValid(uint8_t* buffer, size_t filesize) {
		uint8_t MAGIC[4] = {'L', 'O', 'C', 'K'};
		if (*(uint32_t*)buffer != *(uint32_t*)&MAGIC)
			return false;
		gen = buffer[4];
		if (gen != 3)
			return false;
		masterWrite = buffer[5];
		if (masterWrite > 1)
			return false;
		unsafeCheck = (bool)buffer[7];
		uint8_t start_offset = 0xC;
		if (masterWrite) start_offset += 4;
		if (*(uint32_t*)(&(buffer[8])) != start_offset)
			return false;
		compiledSize = buffer[6] * buffer[6];
		return true;

	}

	Result processBytes(FILE* file) {
		uint32_t main_offset = 0;
		SaltySDCore_fread(&main_offset, 4, 1, file);
		uint8_t value_type = 0;
		SaltySDCore_fread(&value_type, 1, 1, file);
		uint8_t elements = 0;
		SaltySDCore_fread(&elements, 1, 1, file);
		void* temp_buffer = calloc(elements, value_type % 0x10);
		SaltySDCore_fread(temp_buffer, value_type % 0x10, elements, file);
		SaltySD_Memcpy(LOCK::mappings.main_start + main_offset, (u64)temp_buffer, elements * (value_type % 0x10));
		free(temp_buffer);
		return 0;
	}

	Result processVariables(FILE* file) {
		return 0; //TO DO
	}

	Result processCodeCave(FILE* file) {
		return 0; //TO DO
	}

	Result applyMasterWrite(FILE* file, size_t master_offset) {
		uint32_t offset_impl = 0;

		SaltySDCore_fseek(file, master_offset, 0);
		SaltySDCore_fread(&offset_impl, 4, 1, file);
		SaltySDCore_fseek(file, offset_impl, 0);
		if (SaltySDCore_ftell(file) != offset_impl)
			return 0x312;
		
		int8_t OPCODE = 0;
		while(true) {
			SaltySDCore_fread(&OPCODE, 1, 1, file);
			switch(OPCODE) {
				case 1: return processBytes(file);
				case 2: return processVariables(file);
				case 3: return processCodeCave(file);
				case -1: {MasterWriteApplied = true; return 0;}
				default: return 0x355;
			}
		}
	}

	Result writeExprTo(double value, uint8_t* buffer, uint16_t* offset_impl, uint8_t value_type) {
		switch(value_type) {
			case 1:
				buffer[*offset_impl] = (uint8_t)value;
				break;
			case 2:
				*(uint16_t*)(&buffer[*offset_impl]) = (uint16_t)value;
				break;
			case 4:
				*(uint32_t*)(&buffer[*offset_impl]) = (uint32_t)value;
				break;
			case 8:
				*(uint64_t*)(&buffer[*offset_impl]) = (uint64_t)value;
				break;
			case 0x11:
				*(int8_t*)(&buffer[*offset_impl]) = (int8_t)value;
				break;
			case 0x12:
				*(int16_t*)(&buffer[*offset_impl]) = (int16_t)value;
				break;
			case 0x14:
				*(int32_t*)(&buffer[*offset_impl]) = (int32_t)value;
				break;
			case 0x18:
				*(int64_t*)(&buffer[*offset_impl]) = (int64_t)value;
				break;
			case 0x24: {
				#if defined(SWITCH32) || defined(OUNCE32)
				//HOS requires from SIMD load/store instructions to have aligned pointers in A32 mode, so we must avoid using VSTR here
				float new_value = (float)value;
				memcpy(&buffer[*offset_impl], &new_value, 4);
				#else
				*(float*)(&buffer[*offset_impl]) = (float)value;
				#endif
				break;
			}
			case 0x28:
			case 0x38:
				#if defined(SWITCH32) || defined(OUNCE32)
				//HOS requires from SIMD load/store instructions to have aligned pointers in A32 mode, so we must avoid using VSTR here
				memcpy(&buffer[*offset_impl], &value, 8);
				#else
				*(double*)(&buffer[*offset_impl]) = (double)value;
				#endif
				break;
			default:
				return 4;
		}
		*offset_impl += value_type % 0x10;
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
			uint8_t OPCODE = read8(in_buffer);
			if (OPCODE == 1 || OPCODE == 0x81) {
				bool evaluate = false;
				if (OPCODE == 0x81) {
					evaluate = true;
					OPCODE = 1;
				}
				out_buffer[temp_offset++] = OPCODE;
				uint8_t address_count = read8(in_buffer);
				out_buffer[temp_offset++] = address_count;
				out_buffer[temp_offset++] = read8(in_buffer);
				for (size_t i = 1; i < address_count; i++) {
					*(uint32_t*)&out_buffer[temp_offset] = read32(in_buffer);
					temp_offset += 4;
				}
				uint8_t value_type = read8(in_buffer);
				out_buffer[temp_offset++] = value_type;
				uint8_t value_count = read8(in_buffer);
				out_buffer[temp_offset++] = value_count;
				if (!evaluate) for (size_t i = 0; i < value_count; i++) {
					memcpy(&out_buffer[temp_offset], &in_buffer[offset], value_type % 0x10);
					offset += value_type % 0x10;
					temp_offset += value_type % 0x10;
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
				uint8_t address_count = read8(in_buffer);
				out_buffer[temp_offset++] = address_count;
				out_buffer[temp_offset++] = read8(in_buffer); //compare address region
				for (size_t i = 1; i < address_count; i++) {
					*(uint32_t*)&out_buffer[temp_offset] = read32(in_buffer);
					temp_offset += 4;
				}
				out_buffer[temp_offset++] = read8(in_buffer); //compare_type
				uint8_t value_type = read8(in_buffer);
				out_buffer[temp_offset++] = value_type;
				memcpy(&out_buffer[temp_offset], &in_buffer[offset], value_type % 0x10);
				temp_offset += value_type % 0x10;
				offset += value_type % 0x10;
				address_count = read8(in_buffer);
				out_buffer[temp_offset++] = address_count;
				out_buffer[temp_offset++] = read8(in_buffer); //address region
				for (size_t i = 1; i < address_count; i++) {
					*(uint32_t*)&out_buffer[temp_offset] = read32(in_buffer);
					temp_offset += 4;
				}
				value_type = read8(in_buffer);
				out_buffer[temp_offset++] = value_type;
				uint8_t value_count = read8(in_buffer);
				out_buffer[temp_offset++] = value_count;
				if (!evaluate) for (size_t i = 0; i < value_count; i++) {
					memcpy(&out_buffer[temp_offset], &in_buffer[offset], value_type % 0x10);
					offset += value_type % 0x10;
					temp_offset += value_type % 0x10;
				}
				else for (size_t i = 0; i < value_count; i++) {
					double evaluated_value = evaluateExpression((const char*)&in_buffer[offset], (double)FPS, (double)refreshRate);
					offset += strlen((const char*)&in_buffer[offset]) + 1;
					writeExprTo(evaluated_value, out_buffer, &temp_offset, value_type);
				}
			}
			else if (OPCODE == 3) {
				out_buffer[temp_offset++] = OPCODE;
				out_buffer[temp_offset++] = read8(in_buffer);
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
			int8_t OPCODE = read8(buffer);
			if (OPCODE == 1) {
				uint8_t offsets_count = read8(buffer);
				int64_t address = getAddress(buffer, offsets_count);
				if (address < 0) 
					return 6;
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
				uint8_t value_type = read8(buffer);
				uint8_t loops = read8(buffer);
				switch(value_type) {
					case 1:
					case 0x11: {
						for (uint8_t i = 0; i < loops; i++) {
							*(uint8_t*)address = read8(buffer);
							address += 1;
						}
						break;
					}
					case 2:
					case 0x12: {
						for (uint8_t i = 0; i < loops; i++) {
							*(uint16_t*)address = read16(buffer);
							address += 2;
						}
						break;
					}
					case 4:
					case 0x14:
					case 0x24: {
						for (uint8_t i = 0; i < loops; i++) {
							*(uint32_t*)address = read32(buffer);
							address += 4;
						}
						break;
					}
					case 8:
					case 0x18:
					case 0x28: {
						for (uint8_t i = 0; i < loops; i++) {
							*(uint64_t*)address = read64(buffer);
							address += 8;
						}
						break;
					}
					case 0x38: {
						for (uint8_t i = 0; i < loops; i++) {
							overwriteRefreshRate = readDouble(buffer);
							address += 8;
						}
						break;
					}		
					default:
						return 3;
				}
			}
			else if (OPCODE == 2) {
				uint8_t offsets_count = read8(buffer);
				int64_t address = getAddress(buffer, offsets_count);
				if (address < 0) 
					return 6;

				/* compare_type:
					1	=	>
					2	=	>=
					3	=	<
					4	=	<=
					5	=	==
					6	=	!=
				*/
				uint8_t compare_type = read8(buffer);
				uint8_t value_type = read8(buffer);
				bool passed = false;
				switch(value_type) {
					case 1: {
						uint8_t uint8_compare = *(uint8_t*)address;
						uint8_t uint8_tocompare = read8(buffer);
						passed = compareValues(uint8_compare, uint8_tocompare, compare_type);
						break;
					}
					case 2: {
						uint16_t uint16_compare = *(uint16_t*)address;
						uint16_t uint16_tocompare = read16(buffer);
						passed = compareValues(uint16_compare, uint16_tocompare, compare_type);
						break;
					}
					case 4: {
						uint32_t uint32_compare = *(uint32_t*)address;
						uint32_t uint32_tocompare = read32(buffer);
						passed = compareValues(uint32_compare, uint32_tocompare, compare_type);
						break;
					}
					case 8: {
						uint64_t uint64_compare = *(uint64_t*)address;
						uint64_t uint64_tocompare = read64(buffer);
						passed = compareValues(uint64_compare, uint64_tocompare, compare_type);
						break;
					}
					case 0x11: {
						int8_t int8_compare = *(int8_t*)address;
						int8_t int8_tocompare = (int8_t)read8(buffer);
						passed = compareValues(int8_compare, int8_tocompare, compare_type);
						break;
					}
					case 0x12: {
						int16_t int16_compare = *(int16_t*)address;
						int16_t int16_tocompare = (int16_t)read16(buffer);
						passed = compareValues(int16_compare, int16_tocompare, compare_type);
						break;
					}
					case 0x14: {
						int32_t int32_compare = *(int32_t*)address;
						int32_t int32_tocompare = (int32_t)read32(buffer);
						passed = compareValues(int32_compare, int32_tocompare, compare_type);
						break;
					}
					case 0x18: {
						int64_t int64_compare = *(int64_t*)address;
						int64_t int64_tocompare = (int64_t)read64(buffer);
						passed = compareValues(int64_compare, int64_tocompare, compare_type);
						break;
					}
					case 0x24: {
						float float_compare = *(float*)address;
						float float_tocompare = readFloat(buffer);
						passed = compareValues(float_compare, float_tocompare, compare_type);
						break;
					}
					case 0x28: {
						double double_compare = *(double*)address;
						double double_tocompare = readDouble(buffer);
						passed = compareValues(double_compare, double_tocompare, compare_type);
						break;
					}
					default:
						return 3;
				}

				offsets_count = read8(buffer);
				address = getAddress(buffer, offsets_count);
				if (address < 0) 
					return 6;
				value_type = read8(buffer);
				uint8_t loops = read8(buffer);
				switch(value_type) {
					case 1:
					case 0x11: {
						for (uint8_t i = 0; i < loops; i++) {
							uint8_t value8 = read8(buffer);
							if (passed) writeValue(value8, address);
							address += 1;
						}
						break;
					}
					case 2:
					case 0x12: {
						for (uint8_t i = 0; i < loops; i++) {
							uint16_t value16 = read16(buffer);
							if (passed) writeValue(value16, address);
							address += 2;
						}
						break;
					}
					case 4:
					case 0x14:
					case 0x24: {
						for (uint8_t i = 0; i < loops; i++) {
							uint32_t value32 = read32(buffer);
							if (passed) writeValue(value32, address);
							address += 4;
						}
						break;
					}
					case 8:
					case 0x18:
					case 0x28: {
						for (uint8_t i = 0; i < loops; i++) {
							uint64_t value64 = read64(buffer);
							if (passed) writeValue(value64, address);
							address += 8;
						}
						break;
					}
					case 0x38:
						for (uint8_t i = 0; i < loops; i++) {
							uint64_t valueDouble = read64(buffer);
							if (passed) writeValue(valueDouble, (uint64_t)&overwriteRefreshRate);
							address += 8;
						}
						break;						
					default:
						return 3;
				}
			}
			else if (OPCODE == 3) {
				switch(read8(buffer)) {
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
