"""Execute the production launcher state machine, independent of an Android runtime."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
GAME_PACKAGE = 'vcop2'
SOURCE = ROOT / ('android/app/src/main/java/com/vrlightgunhub/' + GAME_PACKAGE + '/ReadyLaunchPolicy.java')
JDK = Path(os.environ.get('JAVA_HOME', 'E:/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2'))

class LauncherPolicyTests(unittest.TestCase):
    def test_setup_requests_the_shared_wide_resizable_quest_panel(self):
        android = '{http://schemas.android.com/apk/res/android}'
        app = ET.parse(ROOT/'android/app/src/main/AndroidManifest.xml').find('application')
        setup = next(activity for activity in app.findall('activity') if activity.get(android+'name') == '.SetupActivity')
        self.assertEqual(setup.get(android+'resizeableActivity'), 'true')
        layout = setup.find('layout')
        self.assertIsNotNone(layout, 'Quest needs explicit default panel dimensions')
        self.assertEqual(layout.get(android+'defaultWidth'), '860dp')
        self.assertEqual(layout.get(android+'defaultHeight'), '640dp')

    def test_real_ready_policy_and_activity_lifecycle(self):
        self.assertTrue(SOURCE.is_file(), 'Launcher has no shared readiness/lifecycle policy yet')
        temporary_root = ROOT/'research/android-tests'
        temporary_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='launcher-', dir=temporary_root) as temporary:
            compilation = subprocess.run([str(JDK/'bin/javac.exe'), '-encoding', 'UTF-8', '-d', temporary, str(SOURCE),
                            str(ROOT/('tests/java/com/vrlightgunhub/'+GAME_PACKAGE+'/LauncherPolicyHarness.java'))],
                           capture_output=True, text=True)
            self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
            result = subprocess.run([str(JDK/'bin/java.exe'), '-cp', temporary,
                                     'com.vrlightgunhub.'+GAME_PACKAGE+'.LauncherPolicyHarness'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS launcher readiness', result.stdout)

if __name__ == '__main__': unittest.main()
