#!/usr/bin/env python3
"""Clip preflight for the R12 video input.

Checks the machine-checkable rows of the video-input spec
(``ADA_ECU/doc/research_notes/video-source-for-r12.md`` §3) against a clip:
container MP4, codec H.264, resolution 1280x720, constant frame rate 20 fps,
duration within bounds, file size <= 60 MB, no audio track — plus a decode
pass proving OpenCV reads >= 99% of the declared frame count (KPI 2).

Every expected value comes from a CLI flag or a ``CLIP_SPEC_*`` env var; the
defaults live in the single ``SpecDefaults`` table below and nowhere else.

Probing prefers ``ffprobe`` when it is on PATH and falls back to OpenCV
properties with a stated notice. The OpenCV fallback cannot observe an audio
track or prove frame-rate constancy; those checks are reported as SKIP, never
as a false PASS.

This script does NOT verify the content rows of the spec (B occluding the
lane, no decoy vehicle, C never visible) — those are human judgements
recorded in the clip's ``*.source.md`` sidecar.

Exit codes: 0 all checks pass (one-line summary on stdout); 1 at least one
check fails (every failing attribute listed actual-vs-expected on stdout);
2 usage or environment error (missing file, missing OpenCV, forced probe
unavailable).
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType
from typing import Final, Mapping, Optional, Sequence

ENV_PREFIX: Final[str] = "CLIP_SPEC_"


@dataclass(frozen=True)
class SpecDefaults:
    """The single defaults table — every expected value, no literals in logic.

    Duration bounds 10-120 s are the interim default: the committed clip is
    10.0 s / 200 frames, and longer runs come from looping, not longer
    footage (sidecar § The remaining deviation; plan 12.2.9.1).
    """

    container: str = "mp4"
    codec: str = "h264"
    width: int = 1280
    height: int = 720
    fps: float = 20.0
    fps_tolerance: float = 0.1
    min_duration_s: float = 10.0
    max_duration_s: float = 120.0
    max_size_mb: float = 60.0
    min_decode_ratio: float = 0.99
    allow_audio: bool = False
    probe: str = "auto"


DEFAULTS: Final[SpecDefaults] = SpecDefaults()

# Codec spellings per probe backend, keyed by the expected-codec token.
_FFPROBE_CODEC_NAMES: Final[Mapping[str, tuple[str, ...]]] = MappingProxyType(
    {
        "h264": ("h264",),
        "mp4v": ("mpeg4",),
    }
)
_FOURCC_CODEC_NAMES: Final[Mapping[str, tuple[str, ...]]] = MappingProxyType(
    {
        "h264": ("avc1", "avc3", "h264", "x264"),
        "mp4v": ("mp4v", "fmp4"),
    }
)

_MB: Final[int] = 1024 * 1024


class ProbeError(RuntimeError):
    """A probe backend was requested but is unavailable or failed."""


@dataclass(frozen=True)
class Probe:
    """What one probe backend could observe about the clip."""

    source: str  # "ffprobe" | "opencv"
    container: Optional[str]  # ffprobe format_name; None under OpenCV
    codec: Optional[str]  # ffprobe codec_name or OpenCV FOURCC, lowercased
    width: Optional[int]
    height: Optional[int]
    fps: Optional[float]
    fps_constant: Optional[bool]  # None = not observable by this backend
    declared_frames: Optional[int]
    duration_s: Optional[float]
    has_audio: Optional[bool]  # None = not observable by this backend


@dataclass(frozen=True)
class Expectations:
    """The expected values a run checks against, resolved from CLI/env."""

    container: str
    codec: str
    width: int
    height: int
    fps: float
    fps_tolerance: float
    min_duration_s: float
    max_duration_s: float
    max_size_mb: float
    min_decode_ratio: float
    allow_audio: bool


@dataclass(frozen=True)
class CheckResult:
    attribute: str
    status: str  # "PASS" | "FAIL" | "SKIP"
    actual: str
    expected: str
    note: str = ""


def _env_str(name: str, default: str) -> str:
    value = os.environ.get(ENV_PREFIX + name)
    return value if value is not None else default


def _env_int(name: str, default: int) -> int:
    value = os.environ.get(ENV_PREFIX + name)
    return int(value) if value is not None else default


def _env_float(name: str, default: float) -> float:
    value = os.environ.get(ENV_PREFIX + name)
    return float(value) if value is not None else default


def _env_bool(name: str, default: bool) -> bool:
    value = os.environ.get(ENV_PREFIX + name)
    if value is None:
        return default
    return value.strip().lower() in ("1", "true", "yes", "on")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("clip", type=Path, help="path to the video clip to check")
    parser.add_argument(
        "--expect-container",
        default=_env_str("CONTAINER", DEFAULTS.container),
        help=f"expected container (default {DEFAULTS.container}; env {ENV_PREFIX}CONTAINER)",
    )
    parser.add_argument(
        "--expect-codec",
        default=_env_str("CODEC", DEFAULTS.codec),
        help=f"expected video codec token (default {DEFAULTS.codec}; env {ENV_PREFIX}CODEC)",
    )
    parser.add_argument(
        "--expect-width",
        type=int,
        default=_env_int("WIDTH", DEFAULTS.width),
        help=f"expected frame width (default {DEFAULTS.width}; env {ENV_PREFIX}WIDTH)",
    )
    parser.add_argument(
        "--expect-height",
        type=int,
        default=_env_int("HEIGHT", DEFAULTS.height),
        help=f"expected frame height (default {DEFAULTS.height}; env {ENV_PREFIX}HEIGHT)",
    )
    parser.add_argument(
        "--expect-fps",
        type=float,
        default=_env_float("FPS", DEFAULTS.fps),
        help=f"expected constant frame rate (default {DEFAULTS.fps}; env {ENV_PREFIX}FPS)",
    )
    parser.add_argument(
        "--fps-tolerance",
        type=float,
        default=_env_float("FPS_TOLERANCE", DEFAULTS.fps_tolerance),
        help=f"absolute fps tolerance (default {DEFAULTS.fps_tolerance}; env {ENV_PREFIX}FPS_TOLERANCE)",
    )
    parser.add_argument(
        "--min-duration-s",
        type=float,
        default=_env_float("MIN_DURATION_S", DEFAULTS.min_duration_s),
        help=(
            f"minimum duration in seconds, inclusive (interim default {DEFAULTS.min_duration_s}; "
            f"env {ENV_PREFIX}MIN_DURATION_S)"
        ),
    )
    parser.add_argument(
        "--max-duration-s",
        type=float,
        default=_env_float("MAX_DURATION_S", DEFAULTS.max_duration_s),
        help=(
            f"maximum duration in seconds, inclusive (default {DEFAULTS.max_duration_s}; "
            f"env {ENV_PREFIX}MAX_DURATION_S)"
        ),
    )
    parser.add_argument(
        "--max-size-mb",
        type=float,
        default=_env_float("MAX_SIZE_MB", DEFAULTS.max_size_mb),
        help=f"maximum file size in MiB (default {DEFAULTS.max_size_mb}; env {ENV_PREFIX}MAX_SIZE_MB)",
    )
    parser.add_argument(
        "--min-decode-ratio",
        type=float,
        default=_env_float("MIN_DECODE_RATIO", DEFAULTS.min_decode_ratio),
        help=(
            f"minimum decoded/declared frame ratio (default {DEFAULTS.min_decode_ratio}; "
            f"env {ENV_PREFIX}MIN_DECODE_RATIO)"
        ),
    )
    parser.add_argument(
        "--allow-audio",
        action="store_true",
        default=_env_bool("ALLOW_AUDIO", DEFAULTS.allow_audio),
        help=f"accept an audio track (default {DEFAULTS.allow_audio}; env {ENV_PREFIX}ALLOW_AUDIO)",
    )
    parser.add_argument(
        "--probe",
        choices=("auto", "ffprobe", "opencv"),
        default=_env_str("PROBE", DEFAULTS.probe),
        help=(
            f"probe backend: auto prefers ffprobe when on PATH (default {DEFAULTS.probe}; "
            f"env {ENV_PREFIX}PROBE)"
        ),
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="also print PASS rows, not only FAIL/SKIP",
    )
    return parser


def _require_cv2():  # -> module; typed loosely to keep cv2 an optional import
    try:
        import cv2  # noqa: PLC0415 — imported here so --help works without OpenCV
    except ImportError as exc:
        raise ProbeError(
            "OpenCV (cv2) is required for the decode pass and the ffprobe fallback: "
            f"{exc}"
        ) from exc
    return cv2


def _parse_rate(rate: str) -> Optional[float]:
    """Parse an ffprobe rational such as ``20/1``; None when undefined."""
    try:
        num_s, _, den_s = rate.partition("/")
        num = float(num_s)
        den = float(den_s) if den_s else 1.0
    except ValueError:
        return None
    if den == 0:
        return None
    return num / den


def probe_ffprobe(path: Path) -> Probe:
    ffprobe = shutil.which("ffprobe")
    if ffprobe is None:
        raise ProbeError("ffprobe not found on PATH")
    cmd = [
        ffprobe,
        "-v",
        "error",
        "-print_format",
        "json",
        "-show_format",
        "-show_streams",
        str(path),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise ProbeError(f"ffprobe failed ({proc.returncode}): {proc.stderr.strip()}")
    data = json.loads(proc.stdout)
    streams = data.get("streams", [])
    video = next((s for s in streams if s.get("codec_type") == "video"), None)
    has_audio = any(s.get("codec_type") == "audio" for s in streams)
    if video is None:
        raise ProbeError("ffprobe found no video stream")

    r_rate = _parse_rate(video.get("r_frame_rate", ""))
    avg_rate = _parse_rate(video.get("avg_frame_rate", ""))
    fps = avg_rate if avg_rate else r_rate
    fps_constant: Optional[bool] = None
    if r_rate is not None and avg_rate is not None:
        fps_constant = abs(r_rate - avg_rate) <= DEFAULTS.fps_tolerance

    declared: Optional[int] = None
    nb_frames = video.get("nb_frames")
    if nb_frames is not None:
        try:
            declared = int(nb_frames)
        except ValueError:
            declared = None

    duration: Optional[float] = None
    fmt = data.get("format", {})
    for candidate in (video.get("duration"), fmt.get("duration")):
        if candidate is not None:
            try:
                duration = float(candidate)
                break
            except ValueError:
                continue
    if declared is None and duration is not None and fps:
        declared = round(duration * fps)

    return Probe(
        source="ffprobe",
        container=fmt.get("format_name"),
        codec=str(video.get("codec_name", "")).lower() or None,
        width=video.get("width"),
        height=video.get("height"),
        fps=fps,
        fps_constant=fps_constant,
        declared_frames=declared,
        duration_s=duration,
        has_audio=has_audio,
    )


def _fourcc_to_str(fourcc: float) -> str:
    value = int(fourcc)
    return "".join(chr((value >> (8 * i)) & 0xFF) for i in range(4)).strip("\x00 ")


def probe_opencv(path: Path) -> Probe:
    cv2 = _require_cv2()
    cap = cv2.VideoCapture(str(path))
    try:
        if not cap.isOpened():
            raise ProbeError("OpenCV could not open the file")
        fps = float(cap.get(cv2.CAP_PROP_FPS)) or None
        declared = int(cap.get(cv2.CAP_PROP_FRAME_COUNT)) or None
        duration = (declared / fps) if (declared and fps) else None
        return Probe(
            source="opencv",
            container=None,  # OpenCV does not report the container format
            codec=_fourcc_to_str(cap.get(cv2.CAP_PROP_FOURCC)).lower() or None,
            width=int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)) or None,
            height=int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) or None,
            fps=fps,
            fps_constant=None,  # not observable through VideoCapture properties
            declared_frames=declared,
            duration_s=duration,
            has_audio=None,  # not observable through VideoCapture properties
        )
    finally:
        cap.release()


def decode_all_frames(path: Path, declared: Optional[int]) -> tuple[int, Optional[str]]:
    """Read every frame OpenCV will yield; return (count, error-or-None)."""
    cv2 = _require_cv2()
    cap = cv2.VideoCapture(str(path))
    try:
        if not cap.isOpened():
            return 0, "OpenCV could not open the file"
        hard_cap = (declared * 2 + 1000) if declared else 1_000_000
        count = 0
        while count < hard_cap:
            ok, _frame = cap.read()
            if not ok:
                break
            count += 1
        return count, None
    except Exception as exc:  # a decode error surfaced as an exception
        return 0, f"decode raised {type(exc).__name__}: {exc}"
    finally:
        cap.release()


def _is_isobmff(path: Path) -> bool:
    """True when the file starts with an ISO BMFF ``ftyp`` box (MP4 family)."""
    with path.open("rb") as handle:
        head = handle.read(12)
    return len(head) >= 8 and head[4:8] == b"ftyp"


def run_checks(
    path: Path, expect: Expectations, probe: Probe
) -> tuple[list[CheckResult], list[str]]:
    """Evaluate every machine-checkable spec row; return (results, notices)."""
    results: list[CheckResult] = []
    notices: list[str] = []

    # Container. The ftyp signature check is backend-independent for MP4.
    if expect.container.lower() == "mp4":
        signature_ok = _is_isobmff(path)
        format_ok = True
        actual = "ISO BMFF (ftyp)" if signature_ok else "no ftyp box"
        if probe.container is not None:
            format_ok = "mp4" in probe.container.lower()
            actual += f", format_name={probe.container}"
        results.append(
            CheckResult(
                attribute="container",
                status="PASS" if (signature_ok and format_ok) else "FAIL",
                actual=actual,
                expected="mp4 (ISO BMFF)",
            )
        )
    elif probe.container is not None:
        ok = expect.container.lower() in probe.container.lower()
        results.append(
            CheckResult(
                attribute="container",
                status="PASS" if ok else "FAIL",
                actual=probe.container,
                expected=expect.container,
            )
        )
    else:
        results.append(
            CheckResult(
                attribute="container",
                status="SKIP",
                actual="unknown",
                expected=expect.container,
                note="non-mp4 container identification requires ffprobe",
            )
        )

    # Codec, against the alias table for the active backend.
    codec_key = expect.codec.lower()
    if probe.source == "ffprobe":
        accepted = _FFPROBE_CODEC_NAMES.get(codec_key, (codec_key,))
    else:
        accepted = _FOURCC_CODEC_NAMES.get(codec_key, (codec_key,))
    actual_codec = probe.codec or "unknown"
    results.append(
        CheckResult(
            attribute="codec",
            status="PASS" if actual_codec in accepted else "FAIL",
            actual=actual_codec,
            expected=f"{expect.codec} (accepted: {', '.join(accepted)})",
        )
    )

    # Resolution.
    actual_res = f"{probe.width}x{probe.height}"
    expected_res = f"{expect.width}x{expect.height}"
    results.append(
        CheckResult(
            attribute="resolution",
            status="PASS" if (probe.width, probe.height) == (expect.width, expect.height) else "FAIL",
            actual=actual_res,
            expected=expected_res,
        )
    )

    # Frame-rate value.
    if probe.fps is None:
        results.append(
            CheckResult(
                attribute="frame_rate",
                status="FAIL",
                actual="unknown",
                expected=f"{expect.fps:g} fps",
            )
        )
    else:
        ok = abs(probe.fps - expect.fps) <= expect.fps_tolerance
        results.append(
            CheckResult(
                attribute="frame_rate",
                status="PASS" if ok else "FAIL",
                actual=f"{probe.fps:.2f} fps",
                expected=f"{expect.fps:g} fps (+/-{expect.fps_tolerance:g})",
            )
        )

    # Frame-rate constancy — only ffprobe can observe it.
    if probe.fps_constant is None:
        results.append(
            CheckResult(
                attribute="frame_rate_constant",
                status="SKIP",
                actual="unknown",
                expected="constant",
                note="frame-rate constancy cannot be checked via OpenCV properties; ffprobe required",
            )
        )
    else:
        results.append(
            CheckResult(
                attribute="frame_rate_constant",
                status="PASS" if probe.fps_constant else "FAIL",
                actual="constant" if probe.fps_constant else "variable (r_frame_rate != avg_frame_rate)",
                expected="constant",
            )
        )

    # Duration, inclusive bounds.
    if probe.duration_s is None:
        results.append(
            CheckResult(
                attribute="duration",
                status="FAIL",
                actual="unknown",
                expected=f"{expect.min_duration_s:g}-{expect.max_duration_s:g} s",
            )
        )
    else:
        ok = expect.min_duration_s <= probe.duration_s <= expect.max_duration_s
        results.append(
            CheckResult(
                attribute="duration",
                status="PASS" if ok else "FAIL",
                actual=f"{probe.duration_s:.2f} s",
                expected=f"{expect.min_duration_s:g}-{expect.max_duration_s:g} s",
            )
        )

    # File size.
    size_mb = path.stat().st_size / _MB
    results.append(
        CheckResult(
            attribute="file_size",
            status="PASS" if size_mb <= expect.max_size_mb else "FAIL",
            actual=f"{size_mb:.2f} MiB",
            expected=f"<= {expect.max_size_mb:g} MiB",
        )
    )

    # Audio track.
    if expect.allow_audio:
        results.append(
            CheckResult(
                attribute="audio",
                status="PASS",
                actual="not checked",
                expected="audio allowed",
            )
        )
    elif probe.has_audio is None:
        results.append(
            CheckResult(
                attribute="audio",
                status="SKIP",
                actual="unknown",
                expected="no audio track",
                note="audio-track presence cannot be checked without ffprobe",
            )
        )
        notices.append(
            "NOTICE: audio-track check skipped -- OpenCV cannot observe audio streams; "
            "install ffprobe to close this check"
        )
    else:
        results.append(
            CheckResult(
                attribute="audio",
                status="PASS" if not probe.has_audio else "FAIL",
                actual="audio track present" if probe.has_audio else "no audio track",
                expected="no audio track",
            )
        )

    # Decode pass (KPI 2) — always OpenCV, whichever backend probed.
    decoded, decode_error = decode_all_frames(path, probe.declared_frames)
    if decode_error is not None:
        results.append(
            CheckResult(
                attribute="decode",
                status="FAIL",
                actual=decode_error,
                expected=f">= {expect.min_decode_ratio:.0%} of declared frames decoded",
            )
        )
    elif not probe.declared_frames:
        results.append(
            CheckResult(
                attribute="decode",
                status="FAIL",
                actual=f"{decoded} frames decoded, declared frame count unknown",
                expected=f">= {expect.min_decode_ratio:.0%} of declared frames decoded",
            )
        )
    else:
        ratio = decoded / probe.declared_frames
        results.append(
            CheckResult(
                attribute="decode",
                status="PASS" if ratio >= expect.min_decode_ratio else "FAIL",
                actual=f"{decoded}/{probe.declared_frames} frames ({ratio:.1%})",
                expected=f">= {expect.min_decode_ratio:.0%} of declared frames",
            )
        )

    return results, notices


def _summary_line(path: Path, results: Sequence[CheckResult], probe: Probe) -> str:
    by_attr = {r.attribute: r for r in results}
    skipped = [r.attribute for r in results if r.status == "SKIP"]
    parts = [
        f"OK {path.name}:",
        f"container={by_attr['container'].status.lower() if by_attr['container'].status != 'PASS' else 'mp4'}",
        f"codec={probe.codec or 'unknown'}",
        f"{probe.width}x{probe.height}",
        f"{probe.fps:.2f}fps" if probe.fps is not None else "fps=unknown",
        f"decoded={by_attr['decode'].actual}",
        f"duration={probe.duration_s:.2f}s" if probe.duration_s is not None else "duration=unknown",
        f"size={path.stat().st_size / _MB:.2f}MiB",
        f"probe={probe.source}",
    ]
    if skipped:
        parts.append(f"skipped={','.join(skipped)}")
    return " ".join(parts)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    clip: Path = args.clip
    if not clip.is_file():
        print(f"error: no such file: {clip}", file=sys.stderr)
        return 2

    expect = Expectations(
        container=args.expect_container,
        codec=args.expect_codec,
        width=args.expect_width,
        height=args.expect_height,
        fps=args.expect_fps,
        fps_tolerance=args.fps_tolerance,
        min_duration_s=args.min_duration_s,
        max_duration_s=args.max_duration_s,
        max_size_mb=args.max_size_mb,
        min_decode_ratio=args.min_decode_ratio,
        allow_audio=args.allow_audio,
    )

    notices: list[str] = []
    try:
        if args.probe == "ffprobe":
            probe = probe_ffprobe(clip)
        elif args.probe == "opencv":
            probe = probe_opencv(clip)
        elif shutil.which("ffprobe") is not None:
            probe = probe_ffprobe(clip)
        else:
            notices.append(
                "NOTICE: ffprobe not found on PATH -- falling back to OpenCV properties "
                "(codec via CAP_PROP_FOURCC; frame-rate constancy and audio-track "
                "checks are skipped, not passed)"
            )
            probe = probe_opencv(clip)
    except ProbeError as exc:
        # An unopenable clip is a failing attribute, not a usage error.
        if "could not open" in str(exc):
            if expect.container.lower() == "mp4" and not _is_isobmff(clip):
                print("FAIL container: actual=no ftyp box expected=mp4 (ISO BMFF)")
            print(f"FAIL decode: actual={exc} expected=a clip OpenCV can open")
            return 1
        print(f"error: {exc}", file=sys.stderr)
        return 2

    try:
        results, check_notices = run_checks(clip, expect, probe)
    except ProbeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    notices.extend(check_notices)

    for notice in notices:
        print(notice, file=sys.stderr)

    failures = [r for r in results if r.status == "FAIL"]
    for result in results:
        if result.status == "FAIL":
            print(f"FAIL {result.attribute}: actual={result.actual} expected={result.expected}")
        elif result.status == "SKIP":
            line = f"SKIP {result.attribute}: {result.note}"
            print(line, file=sys.stderr)
        elif args.verbose:
            print(f"PASS {result.attribute}: {result.actual}", file=sys.stderr)

    if failures:
        print(f"{len(failures)} check(s) failed on {clip.name}")
        return 1

    print(_summary_line(clip, results, probe))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
