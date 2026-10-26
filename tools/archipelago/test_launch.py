"""Save isolation, side-effect-free planning and client integrity controls."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import types
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("sms_ap_launcher", Path(__file__).with_name("launch.py"))
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class LauncherTests(unittest.TestCase):
    def test_profiles_cannot_escape_save_root(self):
        for name in ("../card-a", "..", "C:/saves", "a/b", "a\\b", "", "x" * 49):
            with self.subTest(name=name), self.assertRaises(ValueError):
                launcher.profile_path(name)
        self.assertEqual(launcher.profile_path("room-2") , launcher.ROOT / "saves/archipelago/room-2")

    def test_plan_neither_downloads_nor_creates_profile(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(launcher, "ROOT", Path(temp)), \
                patch.object(launcher, "install", side_effect=AssertionError("network used")):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                launcher.launch(types.SimpleNamespace(profile="test", plan=True, server=None, slot=None))
            self.assertTrue(json.loads(output.getvalue())["normal_saves_untouched"])
            self.assertEqual(list(Path(temp).iterdir()), [])

    def test_wrong_download_hash_never_installs(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(launcher, "ROOT", Path(temp)):
            archive = Path(temp) / "bad.zip"
            archive.write_bytes(b"not the verified client")
            with self.assertRaisesRegex(RuntimeError, "SHA-256"):
                launcher.install(archive)
            self.assertFalse((Path(temp) / "third_party/archipelago" / launcher.BUNDLE_NAME).exists())


if __name__ == "__main__":
    unittest.main()
