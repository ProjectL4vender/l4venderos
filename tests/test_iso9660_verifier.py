import os
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERIFIER = os.path.join(ROOT, "tools", "verify_iso9660.py")
BLOCK = 2048


def put_both_endian_32(buffer, offset, value):
    buffer[offset:offset + 4] = struct.pack("<I", value)
    buffer[offset + 4:offset + 8] = struct.pack(">I", value)


def put_both_endian_16(buffer, offset, value):
    buffer[offset:offset + 2] = struct.pack("<H", value)
    buffer[offset + 2:offset + 4] = struct.pack(">H", value)


def make_synthetic_hybrid_iso(path, include_pvd=True):
    # Simulate GRUB's BIOS MBR/system area followed by a valid ISO descriptor
    # sequence. The key regression is that the MBR at byte 0 is not itself a
    # reason to reject an image if the ISO PVD exists at logical block 16.
    blocks = 24
    image = bytearray(blocks * BLOCK)
    image[0:8] = b"GRUBMBR!"
    image[510:512] = b"\x55\xaa"
    if include_pvd:
        pvd = bytearray(BLOCK)
        pvd[0] = 1
        pvd[1:6] = b"CD001"
        pvd[6] = 1
        pvd[40:72] = b"SYSTEMC".ljust(32, b" ")
        put_both_endian_32(pvd, 80, blocks)
        put_both_endian_16(pvd, 128, BLOCK)
        image[16 * BLOCK:17 * BLOCK] = pvd
        terminator = bytearray(BLOCK)
        terminator[0] = 255
        terminator[1:6] = b"CD001"
        terminator[6] = 1
        image[17 * BLOCK:18 * BLOCK] = terminator
    with open(path, "wb") as stream:
        stream.write(image)


class Iso9660VerifierTests(unittest.TestCase):
    def run_verifier(self, path):
        return subprocess.run(
            [sys.executable, VERIFIER, path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )

    def test_accepts_iso_with_hybrid_mbr(self):
        with tempfile.TemporaryDirectory() as temp:
            path = os.path.join(temp, "hybrid.iso")
            make_synthetic_hybrid_iso(path, True)
            result = self.run_verifier(path)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("ISO-9660 PVD verified", result.stdout)

    def test_rejects_raw_mbr_without_iso_descriptors(self):
        with tempfile.TemporaryDirectory() as temp:
            path = os.path.join(temp, "raw.img")
            make_synthetic_hybrid_iso(path, False)
            result = self.run_verifier(path)
            self.assertEqual(result.returncode, 1)
            self.assertIn("no ISO-9660 Primary Volume Descriptor", result.stderr)

    def test_rejects_partial_iso_block(self):
        with tempfile.TemporaryDirectory() as temp:
            path = os.path.join(temp, "partial.iso")
            with open(path, "wb") as stream:
                stream.write(b"x" * (17 * BLOCK + 1))
            result = self.run_verifier(path)
            self.assertEqual(result.returncode, 1)
            self.assertIn("multiple of 2048-byte ISO blocks", result.stderr)


if __name__ == "__main__":
    unittest.main()
