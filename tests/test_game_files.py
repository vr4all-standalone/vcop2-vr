import hashlib,importlib.util,tempfile,unittest,zipfile,zlib
from pathlib import Path
spec=importlib.util.spec_from_file_location('game_files',Path(__file__).resolve().parents[1]/'tools/game_files.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class MediaTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.original=self.root/'original';self.original.mkdir()
        self.payload=b'original game bytes'
        self.expected=[dict(name='canonical.rom',size=len(self.payload),crc32=f'{zlib.crc32(self.payload):08x}',sha1=hashlib.sha1(self.payload).hexdigest(),group='game')]
    def tearDown(self):self.temp.cleanup()
    def archive(self,name='game.zip',members=None):
        target=self.original/name
        with zipfile.ZipFile(target,'w') as z:
            for key,value in (members or [('canonical.rom',self.payload)]):z.writestr(key,value)
        return target
    def check(self,paths,expected=None):return module.validate_archives(paths,expected=self.expected if expected is None else expected)
    def test_valid_payload(self):
        result=self.check([self.archive()]);self.assertTrue(result['ready']);self.assertEqual(result['verified'],1)
    def test_alias_name_is_verified_by_real_payload(self):
        self.assertTrue(self.check([self.archive(members=[('old-name.bin',self.payload)])])['ready'])
    def test_corruption_is_not_accepted(self):
        report=self.check([self.archive(members=[('canonical.rom',b'x'*len(self.payload))])]);self.assertFalse(report['ready']);self.assertEqual(report['missing'],['canonical.rom'])
    def test_wrong_length_is_rejected(self):self.assertFalse(self.check([self.archive(members=[('canonical.rom',self.payload+b'x')])])['ready'])
    def test_missing_board_dependency_is_named(self):
        expected=self.expected+[dict(name='table.rom',size=3,crc32='352441c2',sha1=hashlib.sha1(b'abc').hexdigest(),group='cpu_board')]
        self.assertEqual(self.check([self.archive()],expected)['missing'],['table.rom'])
    def test_supplemental_archive_supplies_board_payload(self):
        expected=self.expected+[dict(name='table.rom',size=3,crc32='352441c2',sha1=hashlib.sha1(b'abc').hexdigest(),group='cpu_board')]
        report=self.check([self.archive(),self.archive('board.zip',[('table.rom',b'abc')])],expected);self.assertTrue(report['ready'])
    def test_unsafe_members_rejected_without_extraction(self):
        for name in ['../escape','/absolute','C:/absolute','a\\b','a/../b']:
            with self.subTest(name=name):self.assertFalse(self.check([self.archive(members=[('canonical.rom',self.payload),(name,b'x')])])['ready'])
    def test_case_duplicate_members_rejected(self):
        self.assertFalse(self.check([self.archive(members=[('canonical.rom',self.payload),('CANONICAL.ROM',self.payload)])])['ready'])
    def test_truncated_zip_is_reported(self):
        path=self.archive();path.write_bytes(path.read_bytes()[:20]);self.assertFalse(self.check([path])['ready'])
    def test_unknown_extra_payload_is_not_required(self):
        self.assertTrue(self.check([self.archive(members=[('canonical.rom',self.payload),('extra',b'x')])])['ready'])
    def test_stage_has_canonical_names_and_preserves_source(self):
        source=self.archive(members=[('old.bin',self.payload)]);before=source.read_bytes();out=self.root/'state/roms/vcop2.zip'
        module.stage_verified([source],out,expected=self.expected)
        self.assertEqual(source.read_bytes(),before)
        with zipfile.ZipFile(out) as z:self.assertEqual(z.namelist(),['canonical.rom']);self.assertEqual(z.read('canonical.rom'),self.payload)
    def test_stage_refuses_original_or_existing_output(self):
        source=self.archive()
        for target in [source,self.original/'other.zip']:
            with self.subTest(target=target),self.assertRaises(ValueError):module.stage_verified([source],target,expected=self.expected)
        out=self.root/'state/vcop2.zip';out.parent.mkdir();out.write_bytes(b'preserve')
        with self.assertRaises(FileExistsError):module.stage_verified([source],out,expected=self.expected)
        self.assertEqual(out.read_bytes(),b'preserve')
    def test_bad_media_leaves_no_staged_output(self):
        source=self.archive(members=[('canonical.rom',b'bad')]);out=self.root/'state/vcop2.zip'
        with self.assertRaises(ValueError):module.stage_verified([source],out,expected=self.expected)
        self.assertFalse(out.exists())

if __name__=='__main__':unittest.main()
