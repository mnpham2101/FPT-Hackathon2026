from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from detector.association import SingleVehicleBAssociation
from detector.distance import PinholeProjector
from detector.emission import emit_jsonl
from detector.frame_source import synthetic_samples
from detector.models import Detection, FrameInput


class FakeBackend:
    def detect(self, frame: FrameInput):
        yield {"frame": frame.frame_index}


class DetectorSeamsTest(unittest.TestCase):
    def test_single_vehicle_policy_selects_dominant_center_vehicle(self) -> None:
        policy = SingleVehicleBAssociation()
        left = Detection(2, "car", 0.9, 0, 0, 300, 300)
        center = Detection(2, "car", 0.8, 490, 0, 790, 300)

        self.assertIs(policy.select([left, center], 1280), center)
        self.assertEqual(policy.track_id, "own:B")

    def test_pinhole_projector_rejects_invalid_measurement(self) -> None:
        with self.assertRaisesRegex(ValueError, "bbox width"):
            PinholeProjector(1.8, 2000.0).project(0.0, 640.0, 1280)

    def test_pinhole_projector_estimates_longitudinal_and_lateral(self) -> None:
        self.assertEqual(
            PinholeProjector(1.8, 2000.0).project(200.0, 740.0, 1280),
            (18.0, 0.9),
        )

    def test_synthetic_source_preserves_frame_timestamps(self) -> None:
        samples = list(synthetic_samples(2, 1000))

        self.assertEqual(
            [(item.frame_index, item.timestamp_ms) for item in samples],
            [(0, 1000), (1, 1100)],
        )

    def test_emitter_uses_backend_seam_and_keeps_jsonl_pure(self) -> None:
        from contextlib import redirect_stdout
        from io import StringIO

        output = StringIO()
        with redirect_stdout(output):
            count = emit_jsonl([FrameInput(7, 1000, 1280, 720)], FakeBackend())

        self.assertEqual(count, 1)
        self.assertEqual(json.loads(output.getvalue()), {"frame": 7})


if __name__ == "__main__":
    unittest.main()
