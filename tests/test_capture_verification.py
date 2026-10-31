import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('verify_captures', Path(__file__).resolve().parents[1] / 'tools/verify_captures.py')
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class CaptureVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        metadata = dict(schema=1, sequence=1, width=2, height=2, rejected=0, polygons=[], blobs=[])
        (self.root / 'metadata.json').write_text(json.dumps(metadata))
        for name, values in [('native', [0xff123456, 0xff654321, 0xff222222, 0xff333333]),
                             ('foreground', [0, 0xff654321, 0, 0xff333333]),
                             ('background', [0xff000000] * 4)]:
            (self.root / (name + '-argb32.bin')).write_bytes(struct.pack('<4I', *values))

    def tearDown(self):
        self.temp.cleanup()

    def test_exact_native_foreground_composition_is_valid(self):
        self.assertEqual(verifier.validate_capture(self.root)['foreground_mismatches'], 0)

    def test_foreground_memory_or_wrong_color_is_rejected(self):
        # A native RGB layer is copied directly. The former palette-index bug
        # emitted unrelated memory; it must never pass as a usable scene.
        (self.root / 'foreground-argb32.bin').write_bytes(struct.pack('<4I', 0, 0x3f800000, 0, 0xff333333))
        with self.assertRaisesRegex(ValueError, 'Foreground differs'):
            verifier.validate_capture(self.root)

    def test_short_raster_is_rejected(self):
        (self.root / 'native-argb32.bin').write_bytes(b'partial')
        with self.assertRaisesRegex(ValueError, 'Incomplete native raster'):
            verifier.validate_capture(self.root)

    def test_rejected_geometry_is_not_complete(self):
        metadata = json.loads((self.root / 'metadata.json').read_text())
        metadata['rejected'] = 1
        (self.root / 'metadata.json').write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, 'Incomplete scene geometry'):
            verifier.validate_capture(self.root)


if __name__ == '__main__':
    unittest.main()
