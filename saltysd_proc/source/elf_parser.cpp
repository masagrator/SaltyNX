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

#include "elf_parser.hpp"
using namespace elf_parser;

std::vector<section_t> Elf_parser::get_sections() {
	Elf64_Ehdr *ehdr = (Elf64_Ehdr*)m_mmap_program;
	Elf64_Shdr *shdr = (Elf64_Shdr*)(m_mmap_program + ehdr->e_shoff);
	int shnum = ehdr->e_shnum;

	Elf64_Shdr *sh_strtab = &shdr[ehdr->e_shstrndx];
	const char *const sh_strtab_p = (char*)m_mmap_program + sh_strtab->sh_offset;

	std::vector<section_t> sections;
	for (int i = 0; i < shnum; ++i) {
		section_t section;
		section.shdr = &shdr[i];
		section.section_name = std::string(sh_strtab_p + shdr[i].sh_name);
		if (section.section_name.compare(0, 6, ".debug") == 0)
			continue;
		section.data = m_mmap_program + shdr[i].sh_offset;
		section.section_index= i;
		section.section_type = get_section_type(shdr[i].sh_type);
		
		sections.push_back(section);
	}
	return sections;
}

std::vector<segment_t> Elf_parser::get_segments() {
	Elf64_Ehdr *ehdr = (Elf64_Ehdr*)m_mmap_program;
	Elf64_Phdr *phdr = (Elf64_Phdr*)(m_mmap_program + ehdr->e_phoff);
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

std::vector<symbol_t> Elf_parser::get_symbols() {
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

		auto total_syms = sec.shdr->sh_size / sizeof(Elf64_Sym);
		auto syms_data = (Elf64_Sym*)(m_mmap_program + sec.shdr->sh_offset);

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

std::vector<relocation_t> Elf_parser::get_relocations() {
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

		if(sec.section_type != "SHT_RELA") 
			continue;

		auto total_relas = sec.shdr->sh_size / sizeof(Elf64_Rela);
		auto relas_data  = (Elf64_Rela*)(m_mmap_program + sec.shdr->sh_offset);

		for (uint32_t i = 0; i < total_relas; ++i) {
			relocation_t rel;
			rel.rela = &relas_data[i];
			rel.section_idx = sec.shdr->sh_info;
			rel.relocation_plt_address = plt_vma_address + (i + 1) * plt_entry_size;
			rel.relocation_section_name = sec.section_name;
			
			relocations.push_back(rel);
		}
	}
	return relocations;
}

uint8_t *Elf_parser::get_memory_map() {
	return m_mmap_program;
}

std::string Elf_parser::get_section_type(int tt) {
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

std::string Elf_parser::get_segment_type(uint32_t &seg_type) {
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

std::string Elf_parser::get_segment_flags(uint32_t &seg_flags) {
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

void Elf_parser::relocate_segment(int num, uint64_t new_addr)
{
	segment_t segment = get_segments()[num];
	
	uint64_t old_vaddr = segment.phdr->p_vaddr;
	uint64_t old_vaddr_end = segment.phdr->p_vaddr + segment.phdr->p_memsz;
	segment.phdr->p_vaddr = new_addr;
	segment.phdr->p_paddr = new_addr;
	
	
	for (auto sec : get_sections())
	{
		if (sec.shdr->sh_addr >= old_vaddr && sec.shdr->sh_addr < old_vaddr_end)
		{
			sec.shdr->sh_addr -= old_vaddr;
			sec.shdr->sh_addr += new_addr;
			
			for (auto sym : get_symbols())
			{
				if (sym.sym->st_shndx == sec.section_index)
				{
					sym.sym->st_value -= old_vaddr;
					sym.sym->st_value += new_addr;
				}
			}
		}
	}
	
	for (auto rel : get_relocations())
	{
		symbol_t sym = get_symbols()[ELF64_R_SYM(rel.rela->r_info)];
		section_t sec = get_sections()[sym.sym->st_shndx];
		section_t rel_sec = get_sections()[rel.section_idx];
		
		int type = ELF64_R_TYPE(rel.rela->r_info);
		uint64_t sa = sym.sym->st_value + rel.rela->r_addend;
		uint64_t p = (rel.rela->r_offset);
		uint64_t sap = sa - p;
		
		if (rel.rela->r_offset >= old_vaddr && rel.rela->r_offset < old_vaddr_end)
		{
			rel.rela->r_offset -= old_vaddr;
			rel.rela->r_offset += new_addr;
		}

		if (type == R_AARCH64_ABS64)
		{
			*(uint64_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) = sa;
		}
		else if (type == R_AARCH64_ABS32)
		{
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) = (uint32_t)sa;
		}
		else if (type == R_AARCH64_ABS16)
		{
			*(uint16_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) = (uint16_t)sa;
		}
		else if (type == R_AARCH64_PREL64)
		{
			*(uint64_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) = sap;
		}
		else if (type == R_AARCH64_PREL32)
		{
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) = (uint32_t)sap;
		}
		else if (type == R_AARCH64_PREL16)
		{
			*(uint16_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) = (uint16_t)sap;
		}
		else if (type == R_AARCH64_ADR_PREL_PG_HI21)
		{
			uint32_t pages = ((PG(sa) - PG(p)) >> 12);
			uint32_t page_lowerpart = pages >> 2;
			uint32_t page_upperpart = pages & 3;
			
			//printf("%x %x\n", page_lowerpart, page_upperpart);

			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) &= ~0x607FFFE0;
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) |= (page_lowerpart << 5) & 0x7FFFE0;
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) |= (page_upperpart << 29);
		}
		else if (type == R_AARCH64_ADD_ABS_LO12_NC)
		{
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) &= ~0x3ffc00;
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) |= ((sa & 0xFFF) << 10) & 0x3ffc00;
		}
		else if (type == R_AARCH64_LDST32_ABS_LO12_NC)
		{
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) &= ~0x3ffc00;
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) |= ((sa & 0xFFC) << 8);
		}
		else if (type == R_AARCH64_LDST64_ABS_LO12_NC)
		{
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) &= ~0x3ffc00;
			*(uint32_t*)(rel_sec.data + rel.rela->r_offset - rel_sec.shdr->sh_addr) |= ((sa & 0xFF8) << 7);
		}
	}
}
