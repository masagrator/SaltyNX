#!/usr/bin/env python3
"""Packs a shader for embedding: optional GLSL minification, then LZ4 compression.

Usage: pack_shader.py [--glsl] <input> <output>

Output layout: 4 bytes little endian decompressed size, then one raw LZ4 block (lz4 block format, no frame).
GLSL gets a terminating NUL before compression so the unpacked buffer is a C string.
--glsl minifies first: comments and indentation removed, code joined, float literals shortened
(0.500 -> .5). Preprocessor lines stay on their own lines and #version stays the first line: the runtime
replaces exactly that line.
Pure Python, no packages needed. The output is only rewritten when it changes, so make doesn't rebuild
for nothing.
"""
import re
import struct
import sys

# ---------------------------------------------------------------- GLSL minifier
WORD = re.compile(r"[A-Za-z0-9_.]")
FLOAT = re.compile(r"(?<![A-Za-z0-9_.])(\d*)\.(\d*)(?![A-Za-z0-9_])")
OPS = set("+-*/%<>=!&|^")  # operator characters that must stay apart (a - -b, a + +b, ...)


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def short_float(m):
    whole, frac = m.group(1).lstrip("0"), m.group(2).rstrip("0")
    return f"{whole}.{frac}" if whole or frac else "0."


def squeeze(code):
    code = FLOAT.sub(short_float, code)
    out = []
    for tok in code.split():
        if out:
            prev, first = out[-1][-1], tok[0]
            if (WORD.match(prev) and WORD.match(first)) or (prev in OPS and first in OPS):
                out.append(" ")
        out.append(tok)
    return "".join(out)


def minify(src):
    result, code = [], []

    def flush():
        if code:
            joined = squeeze(" ".join(code))
            if joined:
                result.append(joined)
            code.clear()

    for line in strip_comments(src).split("\n"):
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#"):
            flush()
            directive = "#" + " ".join(stripped[1:].split())  # "#  define X  y" -> "#define X y"
            if directive.startswith("#define "):
                parts = directive[len("#define "):].split(" ", 1)
                directive = "#define " + parts[0] + (" " + squeeze(parts[1]) if len(parts) > 1 else "")
            result.append(directive)
        else:
            code.append(stripped)
    flush()
    if not result or not result[0].startswith("#version"):
        raise SystemExit("pack_shader: the first line of a GLSL shader must be #version")
    return "\n".join(result) + "\n"


# ---------------------------------------------------------------- LZ4 block compressor
MIN_MATCH = 4
LAST_LITERALS = 5      # the last 5 bytes are always literals
MF_LIMIT = 12          # no match may start in the last 12 bytes
MAX_OFFSET = 65535


def lz4_compress(data):
    n = len(data)
    out = bytearray()
    heads = {}         # 4 byte sequence -> most recent position
    chain = [-1] * n   # previous position with the same 4 bytes
    anchor = 0         # start of pending literals

    def emit(literals_end, match_len, offset):
        lit = literals_end - anchor
        token_pos = len(out)
        out.append(0)
        token = min(lit, 15) << 4
        if lit >= 15:
            rest = lit - 15
            while rest >= 255:
                out.append(255)
                rest -= 255
            out.append(rest)
        out.extend(data[anchor:literals_end])
        if match_len:
            out.extend(struct.pack("<H", offset))
            ml = match_len - MIN_MATCH
            token |= min(ml, 15)
            if ml >= 15:
                rest = ml - 15
                while rest >= 255:
                    out.append(255)
                    rest -= 255
                out.append(rest)
        out[token_pos] = token

    def insert(pos):
        if pos + MIN_MATCH <= n:
            key = data[pos:pos + MIN_MATCH]
            chain[pos] = heads.get(key, -1)
            heads[key] = pos

    def longest(pos):
        # Longest match for pos, searching every earlier occurrence inside the window.
        best_len, best_off = 0, 0
        limit = n - LAST_LITERALS
        cand = heads.get(data[pos:pos + MIN_MATCH], -1)
        while cand >= 0 and pos - cand <= MAX_OFFSET:
            length = 0
            while pos + length < limit and data[cand + length] == data[pos + length]:
                length += 1
            if length > best_len:
                best_len, best_off = length, pos - cand
            cand = chain[cand]
        return best_len, best_off

    pos = 0
    while pos + MF_LIMIT <= n:
        length, offset = longest(pos)
        if length >= MIN_MATCH:
            # Lazy matching: take a literal now if the next position gives a longer match.
            insert(pos)
            next_len, next_off = longest(pos + 1) if pos + 1 + MF_LIMIT <= n else (0, 0)
            lazy = next_len > length + 1
            if lazy:
                pos += 1
                length, offset = next_len, next_off
            emit(pos, length, offset)
            for p in range(pos if lazy else pos + 1, pos + length):
                insert(p)
            pos += length
            anchor = pos
        else:
            insert(pos)
            pos += 1
    emit(n, 0, 0)  # last literals
    return bytes(out)


def lz4_decompress(src, size):
    # Reference decoder, used to verify every packed file before it's written.
    out = bytearray()
    i = 0
    while i < len(src):
        token = src[i]
        i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]
                i += 1
                lit += b
                if b != 255:
                    break
        out += src[i:i + lit]
        i += lit
        if i >= len(src):
            break
        offset = src[i] | (src[i + 1] << 8)
        i += 2
        ml = (token & 15) + MIN_MATCH
        if (token & 15) == 15:
            while True:
                b = src[i]
                i += 1
                ml += b
                if b != 255:
                    break
        start = len(out) - offset
        for k in range(ml):
            out.append(out[start + k])
    if len(out) != size:
        raise SystemExit("pack_shader: LZ4 round trip failed")
    return bytes(out)


def pack(data):
    compressed = lz4_compress(data)
    if lz4_decompress(compressed, len(data)) != data:
        raise SystemExit("pack_shader: LZ4 round trip failed")
    return struct.pack("<I", len(data)) + compressed


def main():
    args = sys.argv[1:]
    options = [a for a in args if a.startswith("--")]
    args = [a for a in args if not a.startswith("--")]
    if len(args) != 2 or options not in ([], ["--glsl"]):
        raise SystemExit(__doc__)
    data = open(args[0], "rb").read()
    if options == ["--glsl"]:
        data = minify(data.decode("utf-8")).encode("utf-8") + b"\0"
    out = pack(data)
    try:
        if open(args[1], "rb").read() == out:
            return
    except FileNotFoundError:
        pass
    with open(args[1], "wb") as f:
        f.write(out)


if __name__ == "__main__":
    main()
