#pragma once
// Unpacks shaders packed by logo/pack_shader.py: 4 bytes little endian decompressed size, then one raw
// LZ4 block. Every read and write is bounds checked, a malformed block just fails.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace Lz4 {
	// Decompressed size from the 4 byte header.
	constexpr size_t UnpackedSize(const unsigned char* packed) {
		return (size_t)packed[0] | ((size_t)packed[1] << 8) | ((size_t)packed[2] << 16) | ((size_t)packed[3] << 24);
	}

	inline bool DecompressBlock(const unsigned char* src, size_t srcSize, unsigned char* dst, size_t dstSize) {
		const unsigned char* ip = src;
		const unsigned char* const ipEnd = src + srcSize;
		unsigned char* op = dst;
		unsigned char* const opEnd = dst + dstSize;
		while (ip < ipEnd) {
			const unsigned token = *ip++;
			size_t literals = token >> 4;
			if (literals == 15) {
				unsigned byte;
				do {
					if (ip >= ipEnd) return false;
					byte = *ip++;
					literals += byte;
				} while (byte == 255);
			}
			if ((size_t)(ipEnd - ip) < literals || (size_t)(opEnd - op) < literals) return false;
			memcpy(op, ip, literals);
			op += literals;
			ip += literals;
			if (ip == ipEnd) break; // the last sequence has literals only

			if (ipEnd - ip < 2) return false;
			const size_t offset = (size_t)ip[0] | ((size_t)ip[1] << 8);
			ip += 2;
			if (offset == 0 || offset > (size_t)(op - dst)) return false;
			size_t length = (token & 15) + 4;
			if ((token & 15) == 15) {
				unsigned byte;
				do {
					if (ip >= ipEnd) return false;
					byte = *ip++;
					length += byte;
				} while (byte == 255);
			}
			if ((size_t)(opEnd - op) < length) return false;
			const unsigned char* match = op - offset;
			for (size_t i = 0; i < length; i++) op[i] = match[i]; // byte by byte: a match may overlap its output
			op += length;
		}
		return op == opEnd;
	}

	inline unsigned char* Unpack(const unsigned char* packed, size_t packedSize, size_t* size) {
		if (packedSize < 4) return nullptr;
		const size_t unpackedSize = UnpackedSize(packed);
		unsigned char* out = (unsigned char*)malloc(unpackedSize ? unpackedSize : 1);
		if (!out) return nullptr;
		if (!DecompressBlock(packed + 4, packedSize - 4, out, unpackedSize)) {
			free(out);
			return nullptr;
		}
		if (size) *size = unpackedSize;
		return out;
	}
}
