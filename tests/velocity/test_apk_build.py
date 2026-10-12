"""Build guards with the real upstream APK; never generate dummy game C/APKs."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("build_velocity_apk", ROOT / "scripts/build_velocity_apk.py")
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)
OFFICIAL = os.environ.get("VELOCITY_OFFICIAL_APK")


class BuildGuards(unittest.TestCase):
    def test_public_checkout_requires_developer_inputs_without_creating_apk(self):
        if list((ROOT / "src/recomp/gen").glob("recomp_[0-9]*.c")):
            self.skipTest("Developer-generated inputs are present; this absence guard is inapplicable")
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(ROOT / "scripts/build_velocity_apk.py"),
                                     "--output-dir", directory], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("This is not a player step", result.stderr)
            self.assertIn("recomp_dispatch.c", result.stderr)
            self.assertFalse(list(Path(directory).iterdir()))

    @unittest.skipUnless(OFFICIAL, "Set VELOCITY_OFFICIAL_APK to the checksum-verified upstream release")
    def test_real_official_arm64_packaging(self):
        native = BUILD.inspect_contents(Path(OFFICIAL), require_velocity=False)
        self.assertEqual(set(native), BUILD.LIBRARIES)

    @unittest.skipUnless(OFFICIAL, "Set VELOCITY_OFFICIAL_APK to the checksum-verified upstream release")
    def test_unmodified_official_native_library_cannot_be_delivered_as_velocity(self):
        with self.assertRaisesRegex(BUILD.BuildBlocked, "Velocity profiling marker missing"):
            BUILD.inspect_contents(Path(OFFICIAL))


if __name__ == "__main__":
    unittest.main()
