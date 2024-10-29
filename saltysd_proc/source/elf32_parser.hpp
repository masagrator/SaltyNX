// MIT License

// Copyright (c) 2018 finixbit

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef H_ELF32_PARSER
#define H_ELF32_PARSER

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>	/* O_RDONLY */
#include <sys/stat.h> /* For the size of the file. , fstat */
#ifdef ELFPARSE_MMAP
#include <sys/mman.h> /* mmap, MAP_PRIVATE */
#endif
#include <vector>
#include <elf.h>	  // Elf32_Shdr
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

namespace elf32_parser {

typedef struct {
	Elf32_Shdr *shdr;
	uint8_t* data;

	int section_index = 0; 
	std::string section_name;
	std::string section_type;
} section_t;

typedef struct {
	Elf32_Phdr *phdr;
	uint8_t* data;

	std::string segment_type, segment_flags;
} segment_t;

typedef struct {
	Elf32_Sym* sym;
	int symbol_num = 0;
	std::string symbol_name, symbol_section;  
	
	
	std::string get_type_str() {
		switch(ELF32_ST_TYPE(sym->st_info)) {
			case 0: return "NOTYPE";
			case 1: return "OBJECT";
			case 2: return "FUNC";
			case 3: return "SECTION";
			case 4: return "FILE";
			case 6: return "TLS";
			case 7: return "NUM";
			case 10: return "LOOS";
			case 12: return "HIOS";
			default: return "UNKNOWN";
		}
	}

	std::string get_bind_str() {
		switch(ELF32_ST_BIND(sym->st_info)) {
			case 0: return "LOCAL";
			case 1: return "GLOBAL";
			case 2: return "WEAK";
			case 3: return "NUM";
			case 10: return "UNIQUE";
			case 12: return "HIOS";
			case 13: return "LOPROC";
			default: return "UNKNOWN";
		}
	}

	std::string get_visibility_str() {
		switch(ELF32_ST_VISIBILITY(sym->st_other)) {
			case 0: return "DEFAULT";
			case 1: return "INTERNAL";
			case 2: return "HIDDEN";
			case 3: return "PROTECTED";
			default: return "UNKNOWN";
		}
	}

	std::string get_index_str() {
		switch(sym->st_shndx) {
			case SHN_ABS: return "ABS";
			case SHN_COMMON: return "COM";
			case SHN_UNDEF: return "UND";
			case SHN_XINDEX: return "COM";
			default: return std::to_string(sym->st_shndx);
		}
	}

} symbol_t;

typedef struct {
	Elf32_Rel* rel;
	std::string   relocation_section_name;
	uint64_t section_idx, relocation_plt_address;
	
	uint64_t get_symbol_value(const std::vector<symbol_t> &syms) {
		uint64_t sym_val = 0;
		for(auto &sym: syms) {
			if(sym.symbol_num == (const int)ELF32_R_SYM(rel->r_info)) {
				sym_val = sym.sym->st_value;
				break;
			}
		}
		
		return sym_val;
	}
	
	std::string get_symbol_name(const std::vector<symbol_t> &syms) {
		std::string sym_name;
		for(auto &sym: syms) {
			if(sym.symbol_num == (const int)ELF32_R_SYM(rel->r_info)) {
				sym_name = sym.symbol_name;
				break;
			}
		}
		
		return sym_name;
	}
	
	std::string get_relocation_type() {
		switch(ELF32_R_TYPE(rel->r_info)) {
			case 23: return "R_ARM_RELATIVE";
			case 2: return "R_ARM_ABS32";
			case 22: return "R_ARM_JUMP_SLOT";
			default: return "OTHERS";
		}
	}

} relocation_t;


class Elf32_parser {
	public:
		Elf32_parser(uint8_t* data): m_program_path(""), m_mmap_program(data) {}
		Elf32_parser (std::string &program_path): m_program_path{program_path} {   
			load_memory_map();
		}
		~Elf32_parser ()
		{
#if ELFPARSE_MMAP
			if (m_mmap_size)
			{
				msync(m_mmap_program, m_mmap_size, MS_SYNC);
				munmap(m_mmap_program, m_mmap_size);
				close(fd);
			}
#endif
		}
		
		std::vector<section_t> get_sections();
		std::vector<segment_t> get_segments();
		std::vector<symbol_t> get_symbols();
		std::vector<relocation_t> get_relocations();
		uint8_t *get_memory_map();
		
		void relocate(uint32_t text_addr, uint32_t data_addr);
		
	private:
		void load_memory_map();

		std::string get_section_type(int tt);

		std::string get_segment_type(uint32_t &seg_type);
		std::string get_segment_flags(uint32_t &seg_flags);

		int fd;
		std::string m_program_path; 
		uint8_t *m_mmap_program;
		size_t m_mmap_size;
};

}
#endif
