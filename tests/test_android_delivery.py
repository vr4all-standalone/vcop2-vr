"""Media setup and software-only delivery checks (no headset or game required)."""
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import warnings
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / 'android/app/src/main/java/com/vrlightgunhub/vcop2/GameFiles.java'
JDK = Path(os.environ.get('JAVA_HOME', 'E:/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2'))
ANDROID = '{http://schemas.android.com/apk/res/android}'
TEMP_ROOT = ROOT/'research/android-tests'
TEMP_ROOT.mkdir(parents=True, exist_ok=True)


class AndroidDeliveryTests(unittest.TestCase):
    def test_independent_setup_and_immersive_tasks(self):
        path = ROOT / 'android/app/src/main/AndroidManifest.xml'
        self.assertTrue(path.is_file(), 'Quest manifest has not been implemented')
        app = ET.parse(path).find('application')
        self.assertEqual(app.get(ANDROID+'label'), 'Virtua Cop 2 - VR4ALL')
        activities = {x.get(ANDROID+'name'): x for x in app.findall('activity')}
        for activity, category in [('.SetupActivity', 'com.oculus.intent.category.2D'), ('.GameActivity', 'org.khronos.openxr.intent.category.IMMERSIVE_HMD')]:
            self.assertIn(category, {c.get(ANDROID+'name') for c in activities[activity].findall('intent-filter/category')})
        self.assertEqual(activities['.GameActivity'].get(ANDROID+'taskAffinity'), 'com.vrlightgunhub.vcop2.vr')

    def test_java_requirements_exactly_match_pinned_game_set(self):
        self.assertTrue(JAVA.is_file(), 'Android ROM validator has not been implemented')
        text = JAVA.read_text()
        required = json.loads((ROOT/'game-set.json').read_text())['files']
        self.assertEqual(len(required), 31)
        for item in required:
            self.assertIn('new Rom("%s", %d, 0x%sL, "%s")' % (item['name'], item['size'], item['crc32'], item['sha1']), text)

    def test_package_rejects_private_game_data_and_wrong_abi(self):
        path = ROOT/'tools/package.py'
        self.assertTrue(path.is_file(), 'Package validator has not been implemented')
        spec = importlib.util.spec_from_file_location('vcop2_package', path)
        package = importlib.util.module_from_spec(spec); spec.loader.exec_module(package)
        for name in ['assets/vcop2.zip', 'roms/segabill.zip', 'assets/epr-18022.ic2', 'userdata/settings.ini', 'private/key.keystore', '../escape', 'C:\\escape', 'assets/game.chd']:
            with self.subTest(name=name), self.assertRaises(ValueError): package.check_names([name])
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            path = Path(temporary)/'fake.apk'
            def write_apk(machine=183, extra=None):
                with zipfile.ZipFile(path, 'w') as z:
                    for name in package.REQUIRED_ANDROID_LIBS:
                        header = bytearray(64); header[:7] = b'\x7fELF\x02\x01\x01'; struct.pack_into('<H', header, 18, machine)
                        z.writestr('lib/arm64-v8a/'+name, header)
                    z.writestr('assets/fonts/Roboto-Bold.ttf', b'\0\1\0\0'+bytes(20000))
                    z.writestr('AndroidManifest.xml', b'manifest')
                    if extra: z.writestr(extra, b'private')
            write_apk(); self.assertFalse(package.check_apk(path)['game_data_included'])
            write_apk(machine=62)
            with self.assertRaisesRegex(ValueError, 'ABI'): package.check_apk(path)
            write_apk(extra='assets/epr-18022.ic2')
            with self.assertRaises(ValueError): package.check_apk(path)

    def test_windows_package_requires_complete_amd64_components(self):
        path = ROOT/'tools/package.py'
        self.assertTrue(path.is_file(), 'Package validator has not been implemented')
        spec = importlib.util.spec_from_file_location('vcop2_package_windows', path)
        package = importlib.util.module_from_spec(spec); spec.loader.exec_module(package)
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            archive = Path(temporary)/'pcvr.zip'
            def write_windows(machine=0x8664, omit=None):
                with zipfile.ZipFile(archive, 'w') as z:
                    for name in package.REQUIRED_WINDOWS:
                        if name==omit: continue
                        header=bytearray(70); header[:2]=b'MZ'; struct.pack_into('<I',header,60,64)
                        header[64:68]=b'PE\0\0'; struct.pack_into('<H',header,68,machine)
                        z.writestr('vcop2-vr-pcvr/'+name,header)
            write_windows(); self.assertEqual(package.check_windows(archive)['native_abi'],'x86_64')
            write_windows(machine=0x14c)
            with self.assertRaisesRegex(ValueError,'ABI'): package.check_windows(archive)
            write_windows(omit='vcop2_xr.dll')
            with self.assertRaisesRegex(ValueError,'Missing'): package.check_windows(archive)

    def test_windows_package_normalizes_mingw_loader_filename(self):
        path=ROOT/'tools/package.py'
        spec=importlib.util.spec_from_file_location('vcop2_package_loader',path)
        package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            directory=Path(temporary)
            binaries=[]
            for name in ['vcop2.exe','vcop2_xr.dll','libopenxr_loader.dll']:
                source=directory/name
                header=bytearray(70);header[:2]=b'MZ';struct.pack_into('<I',header,60,64)
                header[64:68]=b'PE\0\0';struct.pack_into('<H',header,68,0x8664)
                source.write_bytes(header);binaries.append(source)
            imports=subprocess.CompletedProcess([],0,stdout='')
            with patch.object(package.subprocess,'run',return_value=imports):
                archive=package.windows(*binaries,directory,directory/'readobj.exe',directory/'out')
            with zipfile.ZipFile(archive) as z:
                self.assertIn('vcop2-vr-pcvr/openxr_loader.dll',z.namelist())
                self.assertNotIn('vcop2-vr-pcvr/libopenxr_loader.dll',z.namelist())
                self.assertIn('vcop2-vr-pcvr/docs/hud-size.md',z.namelist())
                readme=z.read('vcop2-vr-pcvr/README.md').decode('utf-8')
                linked_notes=re.findall(r'\((docs/release-notes-[^)]+\.md)\)',readme)
                self.assertTrue(linked_notes, 'Candidate README must select its release notes')
                for note in linked_notes:self.assertIn('vcop2-vr-pcvr/'+note,z.namelist())


class JavaMediaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (JDK/'bin/javac.exe').is_file(): raise unittest.SkipTest('Set JAVA_HOME to JDK 21 for executable media validation tests')
        if not JAVA.is_file(): return
        cls.temp = tempfile.TemporaryDirectory(prefix='vcop2-java-', dir=TEMP_ROOT)
        cls.classes = Path(cls.temp.name)/'classes'; cls.classes.mkdir()
        subprocess.run([str(JDK/'bin/javac.exe'), '-d', str(cls.classes), str(JAVA), str(ROOT/'tests/java/com/vrlightgunhub/vcop2/GameFilesHarness.java')], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, 'temp'): cls.temp.cleanup()

    def setUp(self):
        self.assertTrue(JAVA.is_file(), 'Android ROM validator has not been implemented')
        self.work = tempfile.TemporaryDirectory(dir=TEMP_ROOT); self.folder = Path(self.work.name)

    def tearDown(self):
        if hasattr(self, 'work'): self.work.cleanup()

    def archive(self, name, entries):
        path = self.folder/name
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
                for key, value in entries: z.writestr(key, value)
        return path

    def run_validation(self, paths, real=False):
        return subprocess.run([str(JDK/'bin/java.exe'), '-cp', str(self.classes), 'com.vrlightgunhub.vcop2.GameFilesHarness', 'real' if real else 'synthetic', *map(str, paths)], text=True, capture_output=True)

    def test_split_and_complete_archives(self):
        game = self.archive('vcop2.zip', [('game.rom', b'abc')]); board = self.archive('segabill.zip', [('board.rom', b'abc')])
        self.assertEqual(self.run_validation([game, board]).returncode, 0)
        game = self.archive('vcop2.zip', [('game.rom', b'abc'), ('board.rom', b'abc')])
        self.assertEqual(self.run_validation([game]).returncode, 0)

    def test_missing_corrupt_duplicate_unsafe_and_oversized(self):
        cases = [([('game.rom', b'abc')], 'Missing'), ([('game.rom', b'xyz'), ('board.rom', b'abc')], 'checksum'), ([('game.rom', b'abc'), ('GAME.ROM', b'abc')], 'Duplicate'), ([('../game.rom', b'abc')], 'Unsafe'), ([('game.rom', b'abc'*100)], 'size')]
        for entries, message in cases:
            with self.subTest(message=message):
                result = self.run_validation([self.archive('vcop2.zip', entries)])
                self.assertNotEqual(result.returncode, 0); self.assertIn(message.lower(), result.stderr.lower())

    def test_encrypted_and_truncated_archives(self):
        path = self.archive('vcop2.zip', [('game.rom', b'abc'), ('board.rom', b'abc')]); data = bytearray(path.read_bytes())
        for signature, offset in [(b'PK\x03\x04', 6), (b'PK\x01\x02', 8)]:
            start = 0
            while (start := data.find(signature, start)) >= 0:
                data[start+offset] |= 1; start += 4
        path.write_bytes(data); result = self.run_validation([path]); self.assertNotEqual(result.returncode, 0)
        path.write_bytes(data[:20]); self.assertNotEqual(self.run_validation([path]).returncode, 0)

    def test_real_combined_media_when_available(self):
        path = ROOT/'private/native-roms/vcop2.zip'
        if not path.is_file(): self.skipTest('Private media not available')
        before = hashlib.sha256(path.read_bytes()).digest()
        result = self.run_validation([path], real=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(hashlib.sha256(path.read_bytes()).digest(), before)


if __name__ == '__main__': unittest.main()
