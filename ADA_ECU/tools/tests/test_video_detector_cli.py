from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "video_detector.py"


class VideoDetectorCliTest(unittest.TestCase):
    def test_placeholder_backend_stdout_is_jsonl_only(self) -> None:
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--synthetic",
                "2",
                "--backend",
                "placeholder",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        records = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([record["id"] for record in records], ["own:B", "own:B"])

    def test_unknown_backend_is_rejected_by_cli(self) -> None:
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "--synthetic", "1", "--backend", "unknown"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
