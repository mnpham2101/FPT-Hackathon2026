"""12.3.2.2 — frame-source seam tests.

Runs via ``python -m pytest ADA_ECU/detector/tests`` from the repo root (the CI
invocation). Synthetic-source tests need only numpy; the file-source tests
acquire cv2 via ``pytest.importorskip`` inside the fixture/test so they skip
cleanly where cv2 is absent.
"""

import itertools
import subprocess  # used by the 12.3.3.2 appended test
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from frame_source import FileFrameSource, Frame, SyntheticFrameSource  # noqa: E402

CLIP_FRAMES = 40
CLIP_FPS = 20.0
CLIP_W, CLIP_H = 64, 48


@pytest.fixture
def sample_clip(tmp_path):
    """A 40-frame 64x48 20fps mp4v clip with distinct per-frame content."""
    cv2 = pytest.importorskip("cv2")
    path = tmp_path / "clip.mp4"
    writer = cv2.VideoWriter(
        str(path), cv2.VideoWriter_fourcc(*"mp4v"), CLIP_FPS, (CLIP_W, CLIP_H)
    )
    assert writer.isOpened()
    for i in range(CLIP_FRAMES):
        frame = np.full((CLIP_H, CLIP_W, 3), (i * 6) % 256, dtype=np.uint8)
        writer.write(frame)
    writer.release()
    return path


def test_stride_selects_every_nth_decoded_frame(sample_clip):
    source = FileFrameSource(str(sample_clip), stride=4, loop=False)
    frames = list(source.iter_frames())

    assert [f.clip_index for f in frames] == list(range(0, CLIP_FRAMES, 4))
    assert [f.index for f in frames] == list(range(0, CLIP_FRAMES, 4))
    assert all(f.width == CLIP_W and f.height == CLIP_H for f in frames)
    assert all(f.image.shape == (CLIP_H, CLIP_W, 3) for f in frames)
    assert source.declared_fps == pytest.approx(CLIP_FPS)
    # timestamp_ms = index / declared_fps * 1000
    assert frames[1].timestamp_ms == pytest.approx(4 / CLIP_FPS * 1000.0)


def test_loop_restarts_clip_while_index_keeps_accumulating(sample_clip):
    source = FileFrameSource(str(sample_clip), stride=4, loop=True)
    frames = list(itertools.islice(source.iter_frames(), 25))

    assert len(frames) == 25
    indices = [f.index for f in frames]
    assert all(a < b for a, b in zip(indices, indices[1:]))  # strictly increasing
    # clip_index wraps at each loop: 10 sampled frames per 40-frame pass at stride 4.
    assert [f.clip_index for f in frames] == list(range(0, CLIP_FRAMES, 4)) * 2 + list(range(0, 20, 4))
    assert frames[10].clip_index == 0
    assert frames[10].index == CLIP_FRAMES  # second pass starts at the accumulated ordinal
    assert frames[20].index == 2 * CLIP_FRAMES


def test_eof_without_loop_terminates(sample_clip):
    source = FileFrameSource(str(sample_clip), stride=1, loop=False)
    frames = list(source.iter_frames())
    assert len(frames) == CLIP_FRAMES


def test_synthetic_source_yields_declared_count_and_shape():
    source = SyntheticFrameSource(7, width=32, height=16, fps=10.0, stride=2)
    assert source.declared_fps == 10.0

    frames = list(source.iter_frames())
    assert len(frames) == 7
    assert all(isinstance(f, Frame) for f in frames)
    assert all(f.image.shape == (16, 32, 3) for f in frames)
    assert all(f.width == 32 and f.height == 16 for f in frames)
    assert [f.index for f in frames] == [0, 2, 4, 6, 8, 10, 12]
    assert frames[1].timestamp_ms == pytest.approx(2 / 10.0 * 1000.0)


def test_missing_file_raises_naming_the_path(tmp_path):
    pytest.importorskip("cv2")
    missing = tmp_path / "no-such-clip.mp4"
    with pytest.raises(RuntimeError) as excinfo:
        FileFrameSource(str(missing), stride=1, loop=False)
    assert str(missing) in str(excinfo.value)
# 12.3.3.2 test extension — appended to ADA_ECU/detector/tests/test_frame_source.py
# at commit time. Module-level imports it needs are already present there:
# subprocess, sys, Path (pathlib), pytest, and FileFrameSource (from frame_source).


def test_make_sample_video_output_opens_through_file_frame_source(tmp_path):
    pytest.importorskip("cv2")
    import frame_source  # locate the repo from the module under test, not from this file's path

    repo_root = Path(frame_source.__file__).resolve().parents[2]
    script = repo_root / "ADA_ECU" / "tools" / "make_sample_video.py"
    out = tmp_path / "out.mp4"
    result = subprocess.run(
        [sys.executable, str(script), str(out), "--frames", "30"],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
    assert out.exists()

    frames = list(FileFrameSource(str(out), stride=1, loop=False).iter_frames())
    assert len(frames) == 30
