#!/usr/bin/env python3
"""Verify ISO-9660 volume descriptors without being fooled by a hybrid MBR.

A BIOS/USB-hybrid ISO may have GRUB's MBR code in its first 512 bytes. Some
versions of `file` print only that MBR match and do not display the ISO-9660
signature at logical block 16. This verifier checks the ISO volume-descriptor
sequence itself instead of treating `file` output as authoritative.
Compatible with Python 3.6+.
"""
from __future__ import print_function

import os
import struct
import sys

BLOCK_SIZE = 2048
FIRST_DESCRIPTOR_BLOCK = 16
MAX_DESCRIPTOR_BLOCKS = 256


class IsoValidationError(Exception):
    pass


def verify_iso9660(path):
    if not os.path.isfile(path):
        raise IsoValidationError("file does not exist or is not a regular file")

    size = os.path.getsize(path)
    if size < (FIRST_DESCRIPTOR_BLOCK + 1) * BLOCK_SIZE:
        raise IsoValidationError("file is too small to contain an ISO-9660 PVD")
    if size % BLOCK_SIZE != 0:
        raise IsoValidationError("file length is not a multiple of 2048-byte ISO blocks")

    total_file_blocks = size // BLOCK_SIZE
    pvd = None
    pvd_block = None

    with open(path, "rb") as stream:
        end_block = min(total_file_blocks, FIRST_DESCRIPTOR_BLOCK + MAX_DESCRIPTOR_BLOCKS)
        for block_number in range(FIRST_DESCRIPTOR_BLOCK, end_block):
            stream.seek(block_number * BLOCK_SIZE)
            descriptor = stream.read(BLOCK_SIZE)
            if len(descriptor) != BLOCK_SIZE:
                break
            if descriptor[1:6] != b"CD001" or descriptor[6] != 1:
                # Stop on a non-ISO descriptor sequence unless it is still in
                # the reserved area before any descriptor has been found.
                if pvd is not None:
                    break
                continue
            if descriptor[0] == 1:
                pvd = descriptor
                pvd_block = block_number
                break
            if descriptor[0] == 255:
                break

    if pvd is None:
        raise IsoValidationError(
            "no ISO-9660 Primary Volume Descriptor (type 1, CD001, version 1) "
            "found in the volume-descriptor sequence; output may be a raw disk image"
        )

    volume_blocks_le = struct.unpack("<I", pvd[80:84])[0]
    volume_blocks_be = struct.unpack(">I", pvd[84:88])[0]
    block_size_le = struct.unpack("<H", pvd[128:130])[0]
    block_size_be = struct.unpack(">H", pvd[130:132])[0]

    if volume_blocks_le == 0 or volume_blocks_le != volume_blocks_be:
        raise IsoValidationError("PVD contains an invalid or inconsistent volume-space size")
    if block_size_le != BLOCK_SIZE or block_size_be != BLOCK_SIZE:
        raise IsoValidationError("PVD does not declare 2048-byte logical blocks")
    if volume_blocks_le > total_file_blocks:
        raise IsoValidationError(
            "PVD volume-space size extends beyond the actual file length "
            "({0} blocks declared, {1} available)".format(volume_blocks_le, total_file_blocks)
        )

    volume_id = pvd[40:72].decode("ascii", "replace").strip().rstrip("\x00")
    return {
        "size_bytes": size,
        "file_blocks": total_file_blocks,
        "pvd_block": pvd_block,
        "volume_blocks": volume_blocks_le,
        "volume_id": volume_id,
    }


def main(argv):
    if len(argv) != 2:
        print("Usage: verify_iso9660.py IMAGE.iso", file=sys.stderr)
        return 2
    try:
        info = verify_iso9660(argv[1])
    except (IOError, OSError, IsoValidationError) as exc:
        print("ISO-9660 verification FAILED: {0}".format(exc), file=sys.stderr)
        return 1

    print(
        "ISO-9660 PVD verified: block={pvd_block}, volume={volume_blocks} blocks, "
        "file={file_blocks} blocks, volume-id='{volume_id}'.".format(**info)
    )
    print("The system area may contain a hybrid BIOS MBR; that does not invalidate the ISO.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
