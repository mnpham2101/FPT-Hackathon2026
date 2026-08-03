from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "check_evt_log.py"


def event(name: str, payload: dict) -> dict:
    return {"ts": 1000, "event": name, "payload": payload}


def complete_events() -> list[dict]:
    return [
        event("track_transition", {"source": "own_sensor", "state": "tracked", "id": "own:B"}),
        event("track_transition", {"source": "v2x_relayed", "state": "tracked", "id": "v2x:1:2"}),
        event("risk_transition", {"riskState": "high", "trackId": "v2x:1:2"}),
        event(
            "r4_tx",
            {
                "body": {
                    "trackedObjects": [
                        {"source": "own_sensor", "state": "tracked"},
                        {"source": "v2x_relayed", "state": "tracked"},
                    ],
                    "geometry": {"vehicleB": {"x": 12, "y": 0}},
                }
            },
        ),
    ]


class CheckEvtLogTest(unittest.TestCase):
    def run_check(self, events: list[dict]) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = pathlib.Path(temp_dir) / "events.jsonl"
            path.write_text("".join(json.dumps(item) + "\n" for item in events), encoding="utf-8")
            return subprocess.run([sys.executable, str(SCRIPT), str(path)], text=True, capture_output=True, check=False)

    def test_complete_chain_passes(self) -> None:
        result = self.run_check(complete_events())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_missing_r4_fails(self) -> None:
        result = self.run_check(complete_events()[:-1])
        self.assertEqual(result.returncode, 12)

    def test_only_b_fails(self) -> None:
        events = [item for item in complete_events() if item.get("payload", {}).get("source") != "v2x_relayed"]
        result = self.run_check(events)
        self.assertEqual(result.returncode, 11)

    def test_null_vehicle_b_fails(self) -> None:
        events = complete_events()
        events[-1]["payload"]["body"]["geometry"]["vehicleB"] = None
        result = self.run_check(events)
        self.assertEqual(result.returncode, 16)


if __name__ == "__main__":
    unittest.main()
