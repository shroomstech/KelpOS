import hashlib
import io
import json
import os
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]


class KpmCliTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.source = self.root / "demo-source"
        (self.source / "payload/usr/bin").mkdir(parents=True)
        (self.source / "kpm.json").write_text(json.dumps({
            "name": "demo-tool",
            "version": "1.0.0",
            "description": "A test package",
        }))
        executable = self.source / "payload/usr/bin/demo-tool"
        executable.write_text("#!/bin/sh\necho demo\n")
        executable.chmod(0o755)
        self.package = self.root / "demo-tool.kpm"
        self.install_root = self.root / "installed"

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_kpm(self, *arguments, expected=0):
        result = subprocess.run(
            [sys.executable, "-m", "kpm", *map(str, arguments)],
            cwd=PROJECT_ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, expected, result.stderr)
        return result

    def pack_demo(self):
        self.run_kpm("pack", self.source, self.package)

    @staticmethod
    def add_tar_bytes(archive, name, data, mode=0o644):
        info = tarfile.TarInfo(name)
        info.size = len(data)
        info.mode = mode
        archive.addfile(info, io.BytesIO(data))

    def write_archive(self, path, manifest, entries):
        with tarfile.open(path, "w:gz") as archive:
            self.add_tar_bytes(archive, "manifest.json", json.dumps(manifest).encode())
            for name, data, mode in entries:
                self.add_tar_bytes(archive, name, data, mode)

    def test_pack_install_list_remove_and_preserve_executable_mode(self):
        self.pack_demo()
        self.run_kpm("inspect", self.package)
        self.run_kpm("install", self.package, "--root", self.install_root)

        installed_file = self.install_root / "usr/bin/demo-tool"
        self.assertEqual(installed_file.read_text(), "#!/bin/sh\necho demo\n")
        self.assertEqual(installed_file.stat().st_mode & 0o777, 0o755)
        listing = self.run_kpm("list", "--root", self.install_root)
        self.assertIn("demo-tool 1.0.0", listing.stdout)

        self.run_kpm("remove", "demo-tool", "--root", self.install_root)
        self.assertFalse(installed_file.exists())
        self.assertIn("No packages installed", self.run_kpm("list", "--root", self.install_root).stdout)

    def test_install_refuses_to_overwrite_unmanaged_file(self):
        self.pack_demo()
        target = self.install_root / "usr/bin/demo-tool"
        target.parent.mkdir(parents=True)
        target.write_text("user data")

        result = self.run_kpm("install", self.package, "--root", self.install_root, expected=1)
        self.assertIn("refusing to overwrite", result.stderr)
        self.assertEqual(target.read_text(), "user data")

    def test_remove_refuses_to_delete_modified_file(self):
        self.pack_demo()
        self.run_kpm("install", self.package, "--root", self.install_root)
        target = self.install_root / "usr/bin/demo-tool"
        target.write_text("changed by user")

        result = self.run_kpm("remove", "demo-tool", "--root", self.install_root, expected=1)
        self.assertIn("refusing removal", result.stderr)
        self.assertEqual(target.read_text(), "changed by user")
        self.assertIn("demo-tool 1.0.0", self.run_kpm("list", "--root", self.install_root).stdout)

    def test_inspect_rejects_path_traversal(self):
        package = self.root / "traversal.kpm"
        data = b"escape"
        manifest = {
            "format": 1,
            "name": "bad-package",
            "version": "1",
            "files": [{
                "path": "../escape",
                "sha256": hashlib.sha256(data).hexdigest(),
                "size": len(data),
                "mode": 0o644,
            }],
        }
        self.write_archive(package, manifest, [("payload/../escape", data, 0o644)])
        result = self.run_kpm("inspect", package, expected=1)
        self.assertIn("invalid package path", result.stderr)

    def test_inspect_rejects_symlink_members(self):
        package = self.root / "symlink.kpm"
        with tarfile.open(package, "w:gz") as archive:
            info = tarfile.TarInfo("payload/usr/bin/linked")
            info.type = tarfile.SYMTYPE
            info.linkname = "/etc/passwd"
            archive.addfile(info)
        result = self.run_kpm("inspect", package, expected=1)
        self.assertIn("regular files only", result.stderr)

    def test_inspect_rejects_checksum_mismatch(self):
        package = self.root / "tampered.kpm"
        expected = b"original"
        changed = b"tampered"
        manifest = {
            "format": 1,
            "name": "tampered-package",
            "version": "1",
            "files": [{
                "path": "share/data.txt",
                "sha256": hashlib.sha256(expected).hexdigest(),
                "size": len(expected),
                "mode": 0o644,
            }],
        }
        self.write_archive(package, manifest, [("payload/share/data.txt", changed, 0o644)])
        result = self.run_kpm("inspect", package, expected=1)
        self.assertIn("checksum or size mismatch", result.stderr)

    @unittest.skipUnless(hasattr(os, "symlink"), "symlinks are unavailable")
    def test_pack_rejects_symlink_payload_files(self):
        target = self.source / "payload/usr/bin/linked"
        target.symlink_to("demo-tool")
        result = self.run_kpm("pack", self.source, self.package, expected=1)
        self.assertIn("regular files only", result.stderr)


if __name__ == "__main__":
    unittest.main()