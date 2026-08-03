from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "check_clip_spec.py"
SPEC = importlib.util.spec_from_file_location("check_clip_spec", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def conforming_metadata(**changes: object):
    values = {
        "container_names": frozenset({"mov", "mp4"}),
        "codec_name": "h264",
        "width": 1280,
        "height": 720,
        "average_fps": 20.0,
        "real_fps": 20.0,
        "duration_s": 60.0,
        "size_bytes": 50_000_000,
        "has_audio": False,
        "declared_frames": 1200,
    }
    values.update(changes)
    return MODULE.ClipMetadata(**values)


class CheckClipSpecTest(unittest.TestCase):
    def setUp(self) -> None:
        self.spec = MODULE.ClipSpec(
            "mp4", "h264", 1280, 720, 20.0, 60.0, 120.0, 60.0, 0.99, False
        )

    def test_conforming_metadata_passes(self) -> None:
        self.assertEqual(MODULE.check_metadata(conforming_metadata(), self.spec), [])

    def test_nonconforming_attributes_are_all_named(self) -> None:
        failures = MODULE.check_metadata(
            conforming_metadata(codec_name="hevc", average_fps=25.0, has_audio=True),
            self.spec,
        )
        self.assertEqual(
            [failure.attribute for failure in failures],
            ["codec", "fps", "constant_fps", "audio"],
        )

    def test_cli_names_decode_failure_in_json(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            clip = pathlib.Path(temp_dir) / "clip.mp4"
            clip.touch()
            with (
                mock.patch.object(
                    MODULE,
                    "probe_clip",
                    return_value=(conforming_metadata(), "ffprobe"),
                ),
                mock.patch.object(MODULE, "decode_frame_count", return_value=1000),
                mock.patch("builtins.print") as print_mock,
            ):
                result = MODULE.main([str(clip), "--json"])
        output = json.loads(print_mock.call_args.args[0])
        self.assertEqual(result, 1)
        self.assertEqual(output["failures"][0]["attribute"], "decode")


if __name__ == "__main__":
    unittest.main()
