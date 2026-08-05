"""Tests for the R12 clip preflight ``tools/check_clip_spec.py``.

Synthesizes small conforming and non-conforming clips with OpenCV's
``VideoWriter`` (``mp4v`` — the codec expectation is injectable via
``--expect-codec``/``CLIP_SPEC_CODEC``, so these tests pass while the
real-clip default stays ``h264``) and asserts exit codes and the named
failing attributes. Runs the script as a subprocess, exercising the real CLI.

Verified locally only — no CI lane collects ``ADA_ECU/tools/tests/``.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path
from typing import Mapping, Optional, Sequence

import pytest

cv2 = pytest.importorskip("cv2", reason="OpenCV is required to synthesize test clips")
np = pytest.importorskip("numpy", reason="numpy is required to synthesize test frames")

SCRIPT: Path = Path(__file__).resolve().parents[1] / "check_clip_spec.py"

# Synthesized-clip parameters. VideoWriter here always writes mp4v (MPEG-4
# Part 2) — OpenCV wheels carry no H.264 encoder — hence --expect-codec mp4v
# in every test that should pass the codec check.
SYNTH_CODEC = "mp4v"
CONFORMING = {"width": 1280, "height": 720, "fps": 20.0, "frames": 210}  # 10.5 s
SMALL = {"width": 640, "height": 360, "fps": 20.0, "frames": 210}
SHORT = {"width": 640, "height": 360, "fps": 20.0, "frames": 24}  # 1.2 s
SLOW_FPS = {"width": 640, "height": 360, "fps": 10.0, "frames": 120}  # 12 s


def write_clip(path: Path, *, width: int, height: int, fps: float, frames: int) -> None:
    writer = cv2.VideoWriter(
        str(path), cv2.VideoWriter_fourcc(*SYNTH_CODEC), fps, (width, height)
    )
    assert writer.isOpened(), f"VideoWriter failed to open {path}"
    try:
        for index in range(frames):
            frame = np.full((height, width, 3), (index * 7) % 200, dtype=np.uint8)
            x = 10 + (index * 5) % max(1, width - 80)
            cv2.rectangle(frame, (x, height // 2), (x + 60, height // 2 + 40), (0, 255, 0), -1)
            writer.write(frame)
    finally:
        writer.release()
    assert path.is_file() and path.stat().st_size > 0


def run_script(
    args: Sequence[str], env_extra: Optional[Mapping[str, str]] = None
) -> subprocess.CompletedProcess[str]:
    # Strip any ambient CLIP_SPEC_* so tests control every expectation.
    env = {k: v for k, v in os.environ.items() if not k.startswith("CLIP_SPEC_")}
    if env_extra:
        env.update(env_extra)
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args], capture_output=True, text=True, env=env
    )


@pytest.fixture(scope="session")
def clips_dir(tmp_path_factory: pytest.TempPathFactory) -> Path:
    return tmp_path_factory.mktemp("clips")


@pytest.fixture(scope="session")
def conforming_clip(clips_dir: Path) -> Path:
    path = clips_dir / "conforming.mp4"
    write_clip(path, **CONFORMING)
    return path


@pytest.fixture(scope="session")
def small_clip(clips_dir: Path) -> Path:
    path = clips_dir / "small-640x360.mp4"
    write_clip(path, **SMALL)
    return path


@pytest.fixture(scope="session")
def short_clip(clips_dir: Path) -> Path:
    path = clips_dir / "short-1s.mp4"
    write_clip(path, **SHORT)
    return path


@pytest.fixture(scope="session")
def slow_fps_clip(clips_dir: Path) -> Path:
    path = clips_dir / "slow-10fps.mp4"
    write_clip(path, **SLOW_FPS)
    return path


def test_conforming_clip_exits_0_with_one_line_summary(conforming_clip: Path) -> None:
    proc = run_script([str(conforming_clip), "--expect-codec", SYNTH_CODEC])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    lines = [line for line in proc.stdout.splitlines() if line.strip()]
    assert len(lines) == 1, f"expected exactly one summary line, got: {lines}"
    assert lines[0].startswith(f"OK {conforming_clip.name}:")


def test_default_codec_expectation_rejects_mp4v(conforming_clip: Path) -> None:
    # No --expect-codec: the default stays h264, so a synthesized mp4v clip fails.
    proc = run_script([str(conforming_clip)])
    assert proc.returncode == 1
    assert "FAIL codec:" in proc.stdout
    # OpenCV reports the MPEG-4 Part 2 FOURCC as either mp4v or fmp4.
    assert ("mp4v" in proc.stdout) or ("fmp4" in proc.stdout)  # actual
    assert "h264" in proc.stdout  # expected


def test_wrong_resolution_named_actual_vs_expected(small_clip: Path) -> None:
    proc = run_script(
        [str(small_clip), "--expect-codec", SYNTH_CODEC]
    )
    assert proc.returncode == 1
    assert "FAIL resolution:" in proc.stdout
    assert "640x360" in proc.stdout  # actual
    assert "1280x720" in proc.stdout  # expected


def test_too_short_duration_named(short_clip: Path) -> None:
    proc = run_script(
        [
            str(short_clip),
            "--expect-codec", SYNTH_CODEC,
            "--expect-width", "640",
            "--expect-height", "360",
        ]
    )
    assert proc.returncode == 1
    assert "FAIL duration:" in proc.stdout
    assert "FAIL resolution:" not in proc.stdout  # isolation: only duration fails


def test_frame_rate_mismatch_named(slow_fps_clip: Path) -> None:
    proc = run_script(
        [
            str(slow_fps_clip),
            "--expect-codec", SYNTH_CODEC,
            "--expect-width", "640",
            "--expect-height", "360",
        ]
    )
    assert proc.returncode == 1
    assert "FAIL frame_rate:" in proc.stdout


def test_file_size_limit_named(conforming_clip: Path) -> None:
    proc = run_script(
        [str(conforming_clip), "--expect-codec", SYNTH_CODEC, "--max-size-mb", "0.001"]
    )
    assert proc.returncode == 1
    assert "FAIL file_size:" in proc.stdout


def test_every_failing_attribute_listed(short_clip: Path) -> None:
    # 640x360, mp4v, 1.2 s against pure defaults: codec, resolution and
    # duration must all be listed, not just the first.
    proc = run_script([str(short_clip)])
    assert proc.returncode == 1
    for attribute in ("FAIL codec:", "FAIL resolution:", "FAIL duration:"):
        assert attribute in proc.stdout, f"{attribute} missing from:\n{proc.stdout}"


def test_env_var_overrides_codec_default(conforming_clip: Path) -> None:
    proc = run_script([str(conforming_clip)], env_extra={"CLIP_SPEC_CODEC": SYNTH_CODEC})
    assert proc.returncode == 0, proc.stdout + proc.stderr


def test_cli_flag_beats_env_var(conforming_clip: Path) -> None:
    proc = run_script(
        [str(conforming_clip), "--expect-codec", SYNTH_CODEC],
        env_extra={"CLIP_SPEC_CODEC": "h264"},
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr


def test_garbage_file_fails_naming_container(tmp_path: Path) -> None:
    garbage = tmp_path / "garbage.mp4"
    garbage.write_bytes(b"this is not a video file" * 64)
    proc = run_script([str(garbage)])
    assert proc.returncode == 1
    assert "FAIL container:" in proc.stdout
    assert "FAIL decode:" in proc.stdout


def test_missing_file_is_usage_error(tmp_path: Path) -> None:
    proc = run_script([str(tmp_path / "does-not-exist.mp4")])
    assert proc.returncode == 2


def test_opencv_fallback_skips_audio_honestly(conforming_clip: Path) -> None:
    # Forced OpenCV probe: the audio check must be a stated skip, never a PASS.
    proc = run_script(
        [str(conforming_clip), "--expect-codec", SYNTH_CODEC, "--probe", "opencv"]
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "SKIP audio:" in proc.stderr
    assert "skipped=" in proc.stdout


@pytest.mark.skipif(
    __import__("shutil").which("ffprobe") is None,
    reason="ffprobe not on PATH — probe fallback covered by the forced-opencv test",
)
def test_ffprobe_probe_when_available(conforming_clip: Path) -> None:
    proc = run_script(
        [str(conforming_clip), "--expect-codec", SYNTH_CODEC, "--probe", "ffprobe"]
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "probe=ffprobe" in proc.stdout
