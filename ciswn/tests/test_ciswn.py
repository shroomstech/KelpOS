import os
import signal
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]


class CiswnTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        configured_binary = os.environ.get("CISWN_BIN")
        cls.temporary_directory = None
        if configured_binary:
            cls.binary = Path(configured_binary)
            return
        cls.temporary_directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.temporary_directory.name) / "ciswn"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra",
            "-Werror", "-pedantic", str(PROJECT_ROOT / "ciswn/ciswn.c"), "-lm",
            "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        if cls.temporary_directory is not None:
            cls.temporary_directory.cleanup()

    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.config = self.root / "services.conf"

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_config(self, text):
        self.config.write_text(text)
        return self.config

    def run_ciswn(self, *arguments):
        return subprocess.run(
            [str(self.binary), *map(str, arguments)],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_check_orders_dependencies_before_services(self):
        self.write_config(
            "service worker simple never 0 0 /bin/sleep 1\n"
            "service setup oneshot never 0 0 /bin/true\n"
            "after worker setup\n"
        )
        result = self.run_ciswn("--check", self.config)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("start order: setup worker", result.stdout)

    def test_check_rejects_dependency_cycle(self):
        self.write_config(
            "service alpha simple never 0 0 /bin/true\n"
            "service beta simple never 0 0 /bin/true\n"
            "after alpha beta\n"
            "after beta alpha\n"
        )
        result = self.run_ciswn("--check", self.config)
        self.assertEqual(result.returncode, 2)
        self.assertIn("dependency cycle", result.stderr)

    def test_check_rejects_unknown_dependency(self):
        self.write_config(
            "service alpha simple never 0 0 /bin/true\n"
            "after alpha missing\n"
        )
        result = self.run_ciswn("--check", self.config)
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown dependency", result.stderr)

    def test_oneshot_success_completes(self):
        self.write_config("service prepare oneshot never 0 0 /bin/true\n")
        result = self.run_ciswn(self.config)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("completed prepare", result.stdout)

    def test_oneshot_failure_stops_startup(self):
        self.write_config("service prepare oneshot never 0 0 /bin/false\n")
        result = self.run_ciswn(self.config)
        self.assertEqual(result.returncode, 1)
        self.assertIn("oneshot service prepare failed", result.stderr)

    def test_restart_limit_fails_supervisor(self):
        self.write_config("service broken simple on-failure 1 0 /bin/false\n")
        result = self.run_ciswn(self.config)
        self.assertEqual(result.returncode, 1)
        self.assertIn("exceeded its restart limit", result.stderr)

    def test_sigterm_stops_child_process_group(self):
        self.write_config(
            "shutdown_timeout 1\n"
            "service sleeper simple never 0 0 /bin/sleep 30\n"
        )
        process = subprocess.Popen(
            [str(self.binary), str(self.config)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        try:
            started = process.stdout.readline()
            self.assertIn("started sleeper (pid ", started)
            child_pid = int(started.rsplit("pid ", 1)[1].rstrip(")\n"))
            process.send_signal(signal.SIGTERM)
            stdout, stderr = process.communicate(timeout=4)
            self.assertEqual(process.returncode, 128 + signal.SIGTERM, stderr)
            self.assertNotIn("Traceback", stdout)
            with self.assertRaises(ProcessLookupError):
                os.kill(child_pid, 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)


if __name__ == "__main__":
    unittest.main()