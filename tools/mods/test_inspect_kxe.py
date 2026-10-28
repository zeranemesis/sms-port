import struct
import unittest

from inspect_kxe import inspect


def module():
    data = bytearray(224)
    struct.pack_into('>4sHHII', data, 0, b'KXER', 0, 0, len(data), 0)
    for index in range(6):
        struct.pack_into('>IIIBBH', data, 16 + index * 16, 0xffffffff, 0xffffffff, 0, 32, 0, 0)
    for index, offset, size in ((0, 160, 4), (3, 164, 32), (4, 196, 28)):
        struct.pack_into('>IIIBBH', data, 16 + index * 16, offset, size, 0, 32, 0, 0)
    struct.pack_into('>BBBBIII', data, 164, 10, 0, 0xff, 0, 0, 0, 0)
    struct.pack_into('>BBBBIII', data, 180, 10, 0, 0xfd, 0, 0, 0x80003100, 0)
    struct.pack_into('>III', data, 196, 208, 16, 0)
    data[208:] = b'example_callback'
    return data


class InventoryTests(unittest.TestCase):
    def test_import_strings_are_not_extra_descriptors(self):
        report = inspect(module(), {0x80003100: 'memset'})
        self.assertEqual(report['imports'], {'0': 'example_callback'})
        self.assertEqual(report['retail_calls'], [dict(address='0x80003100', symbol='memset', sites=1)])

    def test_reject_invalid_structures(self):
        for offset, value in ((0, 0), (16, 4096), (112, 4), (164 + 4, 4), (196, 4096)):
            with self.subTest(offset=offset):
                data = module()
                struct.pack_into('>I', data, offset, value)
                with self.assertRaises(ValueError):
                    inspect(data, {})

    def test_reject_truncation(self):
        for size in (0, 15, 64, 163, 223):
            with self.subTest(size=size), self.assertRaises(ValueError):
                inspect(module()[:size], {})


if __name__ == '__main__':
    unittest.main()
