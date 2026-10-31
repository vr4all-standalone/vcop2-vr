import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('diagnostic', Path(__file__).resolve().parents[1] / 'tools/run_diagnostic.py')
diagnostic = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnostic)


class DiagnosticTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.engine = self.root / 'engine.exe'
        self.engine.touch()
        self.roms = self.root / 'media'
        self.roms.mkdir()
        (self.roms / 'vcop2.zip').touch()

    def tearDown(self):
        self.temp.cleanup()

    def invoke(self, error='', mode='gameplay'):
        received = {}

        class Child:
            def wait(self, timeout):
                return 0

        def spawn(args, **kwargs):
            received.update(args=args, **kwargs)
            kwargs['stdout'].write(error)
            kwargs['stdout'].flush()
            return Child()

        output = self.root / 'state'
        with patch.object(diagnostic.subprocess, 'Popen', side_effect=spawn):
            code = diagnostic.run(self.engine, self.roms, output, 1, mode=mode)
        return code, received, json.loads((output / 'run.json').read_text())

    def test_default_gameplay_and_service_load_the_bundled_script(self):
        for mode in ['gameplay', 'service']:
            with self.subTest(mode=mode):
                # Each run owns a fresh directory.
                if (self.root / 'state').exists():
                    self.root = self.root / 'next'
                    self.root.mkdir()
                code, received, report = self.invoke(mode=mode)
                self.assertEqual(code, 0)
                index = received['args'].index('-autoboot_script')
                self.assertTrue(Path(received['args'][index + 1]).is_file())
                self.assertEqual(received['env']['VCOP2_DIAGNOSTIC_MODE'], mode)
                self.assertEqual(report['script'], received['args'][index + 1])

    def test_lua_errors_fail_even_with_a_zero_native_exit(self):
        code, _, report = self.invoke('[LUA ERROR] callback failed\n')
        self.assertEqual(code, 1)
        self.assertFalse(report['diagnostic_ok'])
        self.assertTrue(report['script_errors'])

    def test_original_directory_overlap_is_rejected(self):
        with self.assertRaises(ValueError):
            diagnostic.command(self.engine, self.roms, self.roms / 'state', 1)

    def test_existing_output_is_preserved(self):
        output = self.root / 'state'
        output.mkdir()
        (output / 'keep').write_bytes(b'original')
        with self.assertRaises(FileExistsError):
            diagnostic.command(self.engine, self.roms, output, 1)
        self.assertEqual((output / 'keep').read_bytes(), b'original')

    def test_deadline_range_is_bounded(self):
        for seconds in [0, -1, 601]:
            with self.assertRaises(ValueError):
                diagnostic.command(self.engine, self.roms, self.root / 'state', seconds)


if __name__ == '__main__':
    unittest.main()
