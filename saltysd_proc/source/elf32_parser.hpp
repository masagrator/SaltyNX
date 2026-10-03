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

#include <string>
#include <vector>
#include <elf.h>	  // Elf32_Shdr

namespace elf32_parser {

struct segment_t {
	Elf32_Phdr *phdr;
	uint8_t* data;

	std::string segment_type, segment_flags;
};

class Elf32_parser {
	public:
		Elf32_parser(uint8_t* data): m_mmap_program(data) {}
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
		
		std::vector<segment_t> get_segments();
		uint8_t *get_memory_map();
		
	private:

		std::string get_segment_type(uint32_t &seg_type);
		std::string get_segment_flags(uint32_t &seg_flags);

		int fd;
		uint8_t *m_mmap_program;
		size_t m_mmap_size;
};

}
#endif
