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

uint8_t *Elf32_parser::get_memory_map() {
	return m_mmap_program;
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
