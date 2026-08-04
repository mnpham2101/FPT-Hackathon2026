"""12.3.2.7 — composition-root tests: run() wiring in-process, exit codes and SIGTERM via subprocess.

Runs via ``python -m pytest ADA_ECU/detector/tests`` from the repo root (the CI invocation).
"""

import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import io  # noqa: E402

import main as detector_main  # noqa: E402
from config import load_config  # noqa: E402
from emit import R3Emitter  # noqa: E402
from frame_source import SyntheticFrameSource  # noqa: E402
from inference import Detection  # noqa: E402
from tracker import Tracker  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[3]
MAIN_SCRIPT = REPO_ROOT / "ADA_ECU" / "detector" / "main.py"
MODEL_PATH = REPO_ROOT / "ADA_ECU" / "models" / "yolo11n.onnx"

MODEL_SKIP_REASON = "model not yet committed (12.3.3.1)"


class FakeDetector:
    """One scripted plausible box per frame; one frame carries an extra zero-width box."""

    ZERO_WIDTH_CALL = 3  # 0-based call ordinal that also returns the degenerate box

    def __init__(self):
        self.calls = 0

    def detect(self, image):
        dets = [Detection(bbox_xywh=(300.0, 200.0, 80.0, 60.0), score=0.9, coco_class=2)]
        if self.calls == self.ZERO_WIDTH_CALL:
            # Zero-width box: estimate_range returns None, so this detection must be dropped.
            dets.append(Detection(bbox_xywh=(100.0, 100.0, 0.0, 40.0), score=0.8, coco_class=2))
        self.calls += 1
        return dets


def test_run_in_process_emits_one_line_per_detection():
    config = load_config(env={"DETECTOR_REALTIME_PACING": "false"})
    source = SyntheticFrameSource(count=8)
    fake = FakeDetector()
    stream = io.StringIO()

    rc = detector_main.run(config, source, fake, Tracker(config.track_iou_min), R3Emitter(), stream)

    assert rc == 0
    text = stream.getvalue()
    lines = text.splitlines()
    # 8 frames x 1 plausible box; the zero-width extra on one frame was dropped
    # (9 detections in, 8 lines out) — and nothing but JSONL is in the stream.
    assert len(lines) == 8
    assert text == "".join(line + "\n" for line in lines)
    for line in lines:
        obj = json.loads(line)
        assert obj["source"] == "own_sensor"
        assert obj["id"].startswith("own:")


def _subprocess_env(**overrides):
    env = dict(os.environ)  # keep PATH etc. — cv2/onnxruntime DLLs need it on Windows
    env.update(overrides)
    return env


def test_missing_clip_exits_2_naming_path(tmp_path):
    absent = tmp_path / "absent.mp4"
    proc = subprocess.run(
        [sys.executable, str(MAIN_SCRIPT)],
        cwd=str(REPO_ROOT),
        env=_subprocess_env(
            VIDEO_CLIP_PATH=str(absent),
            DETECTOR_REALTIME_PACING="false",
        ),
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert proc.returncode == 2
    assert str(absent) in proc.stderr
    assert proc.stdout == ""  # stdout stays pure R3 JSONL — nothing on failure


def test_sigterm_terminates_promptly():
    if not MODEL_PATH.is_file():
        pytest.skip(MODEL_SKIP_REASON)

    proc = subprocess.Popen(
        [sys.executable, str(MAIN_SCRIPT), "--synthetic", "100000"],
        cwd=str(REPO_ROOT),
        env=_subprocess_env(
            MODEL_PATH=str(MODEL_PATH),
            DETECTOR_REALTIME_PACING="false",
        ),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        # The startup line is printed (and flushed) once the frame loop is about
        # to start — wait for it so the signal lands after handler installation.
        startup = proc.stderr.readline()
        assert "detector: starting" in startup

        t0 = time.monotonic()
        if os.name == "nt":
            proc.terminate()  # TerminateProcess: not graceful, no handler runs
        else:
            proc.send_signal(signal.SIGTERM)
        proc.communicate(timeout=10)  # raises TimeoutExpired if not bounded
        elapsed = time.monotonic() - t0

        assert elapsed < 10.0
        if os.name != "nt":
            assert proc.returncode == 0
        # On Windows, TerminateProcess kills the process without running the
        # SIGTERM handler, so the graceful exit code cannot be asserted — only
        # bounded termination is checked above.
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.communicate()
