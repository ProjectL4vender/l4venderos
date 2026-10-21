import os
import struct
import tempfile
import unittest

import verify_i386_kernel

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL = os.path.join(ROOT, "systemc-i386.krn")


class KernelVerifierTests(unittest.TestCase):
    def test_bundled_kernel_passes(self):
        result = verify_i386_kernel.parse_kernel(KERNEL)
        self.assertGreater(result["size"], 1000)
        self.assertEqual(result["multiboot_offset"] % 4, 0)

    def test_non_elf_is_rejected(self):
        handle, path = tempfile.mkstemp()
        try:
            os.write(handle, b"not an ELF kernel")
            os.close(handle)
            self.assertRaises(ValueError, verify_i386_kernel.parse_kernel, path)
        finally:
            try:
                os.close(handle)
            except OSError:
                pass
            if os.path.exists(path):
                os.unlink(path)

    def test_wrong_architecture_is_rejected(self):
        with open(KERNEL, "rb") as source:
            data = bytearray(source.read())
        struct.pack_into("<H", data, 18, 62)  # deliberately invalid machine id
        handle, path = tempfile.mkstemp()
        try:
            os.write(handle, bytes(data))
            os.close(handle)
            self.assertRaises(ValueError, verify_i386_kernel.parse_kernel, path)
        finally:
            try:
                os.close(handle)
            except OSError:
                pass
            if os.path.exists(path):
                os.unlink(path)


if __name__ == "__main__":
    unittest.main()
