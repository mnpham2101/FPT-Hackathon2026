from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "check_zero_c.py"


def r3_row(
    *, object_id: str = "own:B", source: str = "own_sensor", x: float = 12.0
) -> dict:
    return {
        "id": object_id,
        "class": "vehicle",
        "source": source,
        "position": {"x": x, "y": 0.0},
        "timestamps": {"measured": 1000},
    }


class CheckZeroCTest(unittest.TestCase):
    def run_check(
        self, rows: list[dict], c_rows: list[dict] | None = None
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temp_dir:
            r3_path = pathlib.Path(temp_dir) / "r3.jsonl"
            r3_path.write_text(
                "".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8"
            )
            command = [sys.executable, str(SCRIPT), str(r3_path)]
            if c_rows is not None:
                c_path = pathlib.Path(temp_dir) / "c.jsonl"
                c_path.write_text(
                    "".join(json.dumps(row) + "\n" for row in c_rows), encoding="utf-8"
                )
                command.extend(["--vehicle-c-log", str(c_path)])
            return subprocess.run(command, text=True, capture_output=True, check=False)

    def test_clean_own_sensor_log_passes(self) -> None:
        result = self.run_check([r3_row()])
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_relayed_source_fails_rule_1(self) -> None:
        result = self.run_check([r3_row(source="v2x_relayed")])
        self.assertEqual(result.returncode, 1)
        self.assertIn("rule 1", result.stderr)

    def test_v2x_namespace_fails_rule_2(self) -> None:
        result = self.run_check([r3_row(object_id="v2x:1201:7")])
        self.assertEqual(result.returncode, 1)
        self.assertIn("rule 2", result.stderr)

    def test_position_near_vehicle_c_fails_rule_3(self) -> None:
        result = self.run_check(
            [r3_row(x=12.0)], [{"timestampMs": 1000, "x": 13.0, "y": 0.0}]
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("rule 3", result.stderr)

    def test_empty_log_is_not_a_vacuous_pass(self) -> None:
        result = self.run_check([])
        self.assertEqual(result.returncode, 2)
        self.assertIn("no R3 objects", result.stderr)


if __name__ == "__main__":
    unittest.main()
