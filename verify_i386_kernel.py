#!/usr/bin/env python3
"""Validate a 32-bit i386 ELF kernel and its Multiboot v1 header.

Compatible with Python 3.6 and newer: deliberately uses no postponed
annotations, modern type-hint syntax, or third-party packages.
"""
from __future__ import print_function
import os
import struct
import sys

ELF_MAGIC = b"\x7fELF"
MULTIBOOT_MAGIC = 0x1BADB002
I386_MACHINE = 3
ELFCLASS32 = 1
ET_EXEC = 2


def fail(message):
    raise ValueError(message)


def parse_kernel(path):
    with open(path, "rb") as stream:
        data = stream.read()

    if len(data) < 52:
        fail("file is too small to be an ELF32 executable")
    if data[:4] != ELF_MAGIC:
        fail("missing ELF magic; GRUB needs the linked ELF kernel, not a raw .bin")
    if data[4] != ELFCLASS32:
        fail("kernel is not ELF32 (i386-only package requires ELFCLASS32)")
    if data[5] != 1:
        fail("kernel is not little-endian ELF")
    if data[6] != 1:
        fail("unsupported ELF identification version")

    fields = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    e_type = fields[0]
    e_machine = fields[1]
    e_entry = fields[3]
    e_phoff = fields[4]
    e_shoff = fields[5]
    e_ehsize = fields[7]
    e_phentsize = fields[8]
    e_phnum = fields[9]
    e_shentsize = fields[10]
    e_shnum = fields[11]
    e_shstrndx = fields[12]

    if e_type != ET_EXEC:
        fail("ELF file is not a statically linked executable")
    if e_machine != I386_MACHINE:
        fail("ELF machine is not Intel 80386 (EM_386)")
    if e_ehsize != 52:
        fail("unexpected ELF32 header size")
    if e_phnum and (e_phentsize != 32 or e_phoff + e_phnum * e_phentsize > len(data)):
        fail("program header table is truncated or malformed")

    # Multiboot v1 requires its header to be 4-byte aligned within the first 8 KiB.
    search_end = min(len(data), 8192)
    header_offset = None
    header_flags = None
    for offset in range(0, search_end - 11, 4):
        magic, flags, checksum = struct.unpack_from("<III", data, offset)
        if magic == MULTIBOOT_MAGIC:
            if (magic + flags + checksum) & 0xFFFFFFFF:
                fail("Multiboot header checksum is invalid")
            header_offset = offset
            header_flags = flags
            break
    if header_offset is None:
        fail("Multiboot v1 header not found at a 4-byte boundary in the first 8192 bytes")
    if header_offset + 28 > len(data):
        fail("Multiboot video request is truncated")
    if header_flags & 0x4:
        mode_type, width, height, depth = struct.unpack_from("<IIII", data, header_offset + 12)
        if mode_type != 0 or width == 0 or height == 0 or depth not in (15, 16, 24, 32):
            fail("invalid Multiboot framebuffer request")

    # Ensure the entry point is covered by a loadable segment.
    entry_is_loadable = False
    for index in range(e_phnum):
        off = e_phoff + index * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from("<IIIIIIII", data, off)
        if p_type == 1 and p_vaddr <= e_entry < p_vaddr + p_memsz:
            entry_is_loadable = True
            break
    if e_phnum and not entry_is_loadable:
        fail("ELF entry point is not inside a PT_LOAD segment")

    # Check that the kernel entry function exists when a symbol table is present.
    if e_shnum:
        if e_shentsize != 40 or e_shoff + e_shnum * e_shentsize > len(data):
            fail("section header table is truncated or malformed")
        sections = []
        for index in range(e_shnum):
            off = e_shoff + index * e_shentsize
            sections.append(struct.unpack_from("<IIIIIIIIII", data, off))
        found_kernel_main = False
        for section in sections:
            sh_type = section[1]
            sh_offset = section[4]
            sh_size = section[5]
            sh_link = section[6]
            sh_entsize = section[9]
            if sh_type != 2:  # SHT_SYMTAB
                continue
            if sh_link >= len(sections) or sh_entsize != 16:
                fail("malformed ELF symbol table")
            str_section = sections[sh_link]
            str_start = str_section[4]
            str_size = str_section[5]
            if str_start + str_size > len(data) or sh_offset + sh_size > len(data):
                fail("symbol or string table exceeds file length")
            strings = data[str_start:str_start + str_size]
            end = sh_offset + sh_size
            for sym_off in range(sh_offset, end, sh_entsize):
                if sym_off + 16 > end:
                    break
                name_offset = struct.unpack_from("<I", data, sym_off)[0]
                if name_offset >= len(strings):
                    continue
                name_end = strings.find(b"\x00", name_offset)
                if name_end < 0:
                    continue
                if strings[name_offset:name_end] == b"kernel_main":
                    found_kernel_main = True
                    break
            if found_kernel_main:
                break
        if not found_kernel_main:
            fail("kernel_main symbol is missing from the linked ELF")

    return {
        "size": len(data),
        "entry": e_entry,
        "multiboot_offset": header_offset,
        "multiboot_flags": header_flags,
    }


def main(argv):
    if len(argv) != 2:
        print("Usage: {} PATH_TO_KERNEL.ELF".format(os.path.basename(argv[0])), file=sys.stderr)
        return 2
    path = argv[1]
    try:
        result = parse_kernel(path)
    except (IOError, OSError, ValueError, struct.error) as exc:
        print("i386 kernel verification FAILED: {}".format(exc), file=sys.stderr)
        return 1
    print("i386 kernel verification PASSED")
    print("  File: {}".format(path))
    print("  Size: {} bytes".format(result["size"]))
    print("  Entry: 0x{:08X}".format(result["entry"]))
    print("  Multiboot header offset: {} bytes".format(result["multiboot_offset"]))
    print("  Multiboot flags: 0x{:08X}".format(result["multiboot_flags"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
