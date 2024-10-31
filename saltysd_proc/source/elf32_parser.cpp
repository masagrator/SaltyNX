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

#include "elf32_parser.hpp"
#include "useful.h"
using namespace elf32_parser;

std::vector<section_t> Elf32_parser::get_sections() {
	Elf32_Ehdr *ehdr = (Elf32_Ehdr*)m_mmap_program;
	Elf32_Shdr *shdr = (Elf32_Shdr*)(m_mmap_program + ehdr->e_shoff);
	int shnum = ehdr->e_shnum;

	Elf32_Shdr *sh_strtab = &shdr[ehdr->e_shstrndx];
	const char *const sh_strtab_p = (char*)m_mmap_program + sh_strtab->sh_offset;

	std::vector<section_t> sections;
	for (int i = 0; i < shnum; ++i) {
		section_t section;
		section.shdr = &shdr[i];
		section.data = m_mmap_program + shdr[i].sh_offset;
		section.section_index= i;
		section.section_name = std::string(sh_strtab_p + shdr[i].sh_name);
		section.section_type = get_section_type(shdr[i].sh_type);

		sections.push_back(section);
	}
	return sections;
}

std::vector<segment_t> Elf32_parser::get_segments() {
	Elf32_Ehdr *ehdr = (Elf32_Ehdr*)m_mmap_program;
	Elf32_Phdr *phdr = (Elf32_Phdr*)(m_mmap_program + ehdr->e_phoff);
	int phnum = ehdr->e_phnum;

	std::vector<segment_t> segments;
	for (int i = 0; i < phnum; ++i) {
		segment_t segment;
		segment.phdr = &phdr[i];
		segment.data = m_mmap_program + phdr[i].p_offset;
		segment.segment_type	 = get_segment_type(phdr[i].p_type);
		segment.segment_flags	= get_segment_flags(phdr[i].p_flags);
		
		segments.push_back(segment);
	}
	return segments;
}

std::vector<symbol_t> Elf32_parser::get_symbols() {
	std::vector<section_t> secs = get_sections();

	// get strtab
	char *sh_strtab_p = nullptr;
	for(auto &sec: secs) {
		if((sec.section_type == "SHT_STRTAB") && (sec.section_name == ".strtab")){
			sh_strtab_p = (char*)m_mmap_program + sec.shdr->sh_offset;
			break;
		}
	}

	// get dynstr
	char *sh_dynstr_p = nullptr;
	for(auto &sec: secs) {
		if((sec.section_type == "SHT_STRTAB") && (sec.section_name == ".dynstr")){
			sh_dynstr_p = (char*)m_mmap_program + sec.shdr->sh_offset;
			break;
		}
	}

	std::vector<symbol_t> symbols;
	for(auto &sec: secs) {
		if((sec.section_type != "SHT_SYMTAB") && (sec.section_type != "SHT_DYNSYM"))
			continue;

		auto total_syms = sec.shdr->sh_size / sizeof(Elf32_Sym);
		auto syms_data = (Elf32_Sym*)(m_mmap_program + sec.shdr->sh_offset);

		for (uint32_t i = 0; i < total_syms; ++i) {
			symbol_t symbol;
			symbol.sym = &syms_data[i];
			symbol.symbol_num	   = i;
			symbol.symbol_section   = sec.section_name;
			
			if(sec.section_type == "SHT_SYMTAB")
				symbol.symbol_name = std::string(sh_strtab_p + syms_data[i].st_name);
			
			if(sec.section_type == "SHT_DYNSYM")
				symbol.symbol_name = std::string(sh_dynstr_p + syms_data[i].st_name);
			
			symbols.push_back(symbol);
		}
	}
	return symbols;
}

std::vector<relocation_t> Elf32_parser::get_relocations() {
	auto secs = get_sections();
	auto syms = get_symbols();
	
	int  plt_entry_size = 0;
	long plt_vma_address = 0;

	for (auto &sec : secs) {
		if(sec.section_name == ".plt") {
		  plt_entry_size = sec.shdr->sh_entsize;
		  plt_vma_address = sec.shdr->sh_addr;
		  break;
		}
	}

	std::vector<relocation_t> relocations;
	for (auto &sec : secs) {

		if(sec.section_type != "SHT_REL") 
			continue;

		auto total_relas = sec.shdr->sh_size / sizeof(Elf32_Rel);
		auto relas_data  = (Elf32_Rel*)(m_mmap_program + sec.shdr->sh_offset);

		for (uint32_t i = 0; i < total_relas; ++i) {
			relocation_t rel;
			rel.rel = &relas_data[i];
			rel.section_idx = sec.shdr->sh_info;
			rel.relocation_plt_address = plt_vma_address + (i + 1) * plt_entry_size;
			rel.relocation_section_name = sec.section_name;
			
			relocations.push_back(rel);
		}
	}
	return relocations;
}

uint8_t *Elf32_parser::get_memory_map() {
	return m_mmap_program;
}

void Elf32_parser::load_memory_map() {
#ifdef ELFPARSE_MMAP
	int i;
	struct stat st;

	if ((fd = open(m_program_path.c_str(), O_RDWR, (mode_t)0600)) < 0) {
		SaltySD_printf("Err: open\n");
		exit(-1);
	}
	if (fstat(fd, &st) < 0) {
		SaltySD_printf("Err: fstat\n");
		exit(-1);
	}
	
	m_mmap_size = st.st_size;
	m_mmap_program = static_cast<uint8_t*>(mmap(NULL, m_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
	if (m_mmap_program == MAP_FAILED) {
		SaltySD_printf("Err: mmap\n");
		exit(-1);
	}

	auto header = (Elf32_Ehdr*)m_mmap_program;
	if (header->e_ident[EI_CLASS] != ELFCLASS32) {
		SaltySD_printf("Only 32-bit files supported\n");
		exit(1);
	}
#endif
}

std::string Elf32_parser::get_section_type(int tt) {
	if(tt < 0)
		return "UNKNOWN";

	switch(tt) {
		case 0: return "SHT_NULL";	  /* Section header table entry unused */
		case 1: return "SHT_PROGBITS";  /* Program data */
		case 2: return "SHT_SYMTAB";	/* Symbol table */
		case 3: return "SHT_STRTAB";	/* String table */
		case 4: return "SHT_RELA";	  /* Relocation entries with addends */
		case 5: return "SHT_HASH";	  /* Symbol hash table */
		case 6: return "SHT_DYNAMIC";   /* Dynamic linking information */
		case 7: return "SHT_NOTE";	  /* Notes */
		case 8: return "SHT_NOBITS";	/* Program space with no data (bss) */
		case 9: return "SHT_REL";	   /* Relocation entries, no addends */
		case 11: return "SHT_DYNSYM";   /* Dynamic linker symbol table */
		default: return "UNKNOWN";
	}
	return "UNKNOWN";
}

std::string Elf32_parser::get_segment_type(uint32_t &seg_type) {
	switch(seg_type) {
		case PT_NULL:   return "NULL";				  /* Program header table entry unused */ 
		case PT_LOAD: return "LOAD";					/* Loadable program segment */
		case PT_DYNAMIC: return "DYNAMIC";			  /* Dynamic linking information */
		case PT_INTERP: return "INTERP";				/* Program interpreter */
		case PT_NOTE: return "NOTE";					/* Auxiliary information */
		case PT_SHLIB: return "SHLIB";				  /* Reserved */
		case PT_PHDR: return "PHDR";					/* Entry for header table itself */
		case PT_TLS: return "TLS";					  /* Thread-local storage segment */
		case PT_NUM: return "NUM";					  /* Number of defined types */
		case PT_LOOS: return "LOOS";					/* Start of OS-specific */
		case PT_GNU_EH_FRAME: return "GNU_EH_FRAME";	/* GCC .eh_frame_hdr segment */
		case PT_GNU_STACK: return "GNU_STACK";		  /* Indicates stack executability */
		case PT_GNU_RELRO: return "GNU_RELRO";		  /* Read-only after relocation */
		//case PT_LOSUNW: return "LOSUNW";
		case PT_SUNWBSS: return "SUNWBSS";			  /* Sun Specific segment */
		case PT_SUNWSTACK: return "SUNWSTACK";		  /* Stack segment */
		//case PT_HISUNW: return "HISUNW";
		case PT_HIOS: return "HIOS";					/* End of OS-specific */
		case PT_LOPROC: return "LOPROC";				/* Start of processor-specific */
		case PT_HIPROC: return "HIPROC";				/* End of processor-specific */
		default: return "UNKNOWN";
	}
}

std::string Elf32_parser::get_segment_flags(uint32_t &seg_flags) {
	std::string flags;

	if(seg_flags & PF_R)
		flags.append("R");

	if(seg_flags & PF_W)
		flags.append("W");

	if(seg_flags & PF_X)
		flags.append("E");

	return flags;
}

#define PG(x) (x & ~0xFFF)

void Elf32_parser::relocate(uint32_t text_addr, uint32_t data_addr, uint32_t read_addr)
{
	auto segs = get_segments();
	uint8_t* RX_segment = 0;
	uint8_t* RW_segment = 0;
	uint8_t* RO_segment = 0;
	uint32_t data_offset = 0;
	uint32_t read_offset = 0;
	for (auto seg: segs) {
		SaltySD_printf("Segment %s, flags: %s, addr: %p, vaddr: %x\n", seg.segment_type.c_str(), seg.segment_flags.c_str(), seg.data, seg.phdr -> p_vaddr);
		if (!RX_segment && seg.segment_flags == "RE") {
			RX_segment = seg.data;
		}
		else if (!RO_segment && seg.segment_flags == "R") {
			RO_segment = seg.data;
			read_offset = seg.phdr -> p_vaddr;
		}
		else if (!RW_segment && seg.segment_flags == "RW") {
			RW_segment = seg.data;
			data_offset = seg.phdr -> p_vaddr;
			break;
		}
	}

	ptrdiff_t data_physical_offset = RW_segment - RX_segment;
	ptrdiff_t read_physical_offset = RO_segment - RX_segment;
	ptrdiff_t data_virt_phys_diff = data_offset - data_physical_offset;
	ptrdiff_t read_virt_phys_diff = read_offset - read_physical_offset;

	for (auto rel : get_relocations())
	{
		symbol_t sym = get_symbols()[ELF32_R_SYM(rel.rel->r_info)];
		
		int type = ELF32_R_TYPE(rel.rel->r_info);

		if (type == R_ARM_ABS32 || type == R_ARM_JUMP_SLOT)
		{
			
		}
		else if (type == R_ARM_RELATIVE) {
			if (rel.rel->r_offset >= data_offset) {
				*(uint32_t*)(RX_segment + rel.rel->r_offset - data_virt_phys_diff) += text_addr;
			}
			else if (read_addr && rel.rel->r_offset >= read_offset) {
				*(uint32_t*)(RX_segment + rel.rel->r_offset - read_virt_phys_diff) += text_addr;
			}
			else {
				*(uint32_t*)(RX_segment + rel.rel->r_offset) += text_addr;
			}
		}
		else SaltySD_printf("32bit UNK_REL: 0x%x\n", type);
	}
}
