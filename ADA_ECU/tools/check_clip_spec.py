#!/usr/bin/env python3
"""Preflight the machine-checkable R12 dashcam clip requirements."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
from dataclasses import dataclass
from fractions import Fraction


@dataclass(frozen=True)
class ClipSpec:
    container: str
    codec: str
    width: int
    height: int
    fps: float
    min_duration_s: float
    max_duration_s: float
    max_size_mb: float
    min_decode_ratio: float
    allow_audio: bool


@dataclass(frozen=True)
class ClipMetadata:
    container_names: frozenset[str]
    codec_name: str
    width: int
    height: int
    average_fps: float
    real_fps: float
    duration_s: float
    size_bytes: int
    has_audio: bool
    declared_frames: int


@dataclass(frozen=True)
class Failure:
    attribute: str
    actual: object
    expected: str

    def message(self) -> str:
        return f"{self.attribute}: actual={self.actual!r}, expected={self.expected}"


def parse_rate(value: str | None) -> float:
    if not value or value == "0/0":
        return 0.0
    try:
        return float(Fraction(value))
    except (ValueError, ZeroDivisionError):
        return 0.0


def probe_with_ffprobe(path: pathlib.Path) -> ClipMetadata:
    command = [
        "ffprobe",
        "-v",
        "error",
        "-show_entries",
        "format=format_name,duration,size:"
        "stream=codec_type,codec_name,width,height,avg_frame_rate,"
        "r_frame_rate,nb_frames,duration",
        "-of",
        "json",
        str(path),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        detail = result.stderr.strip() or "unknown ffprobe error"
        raise RuntimeError(f"ffprobe failed: {detail}")
    body = json.loads(result.stdout)
    streams = body.get("streams", [])
    video = next(
        (stream for stream in streams if stream.get("codec_type") == "video"), None
    )
    if video is None:
        raise RuntimeError("ffprobe found no video stream")
    format_body = body.get("format", {})
    duration_s = float(format_body.get("duration") or video.get("duration") or 0)
    average_fps = parse_rate(video.get("avg_frame_rate"))
    declared_frames = int(video.get("nb_frames") or round(duration_s * average_fps))
    return ClipMetadata(
        container_names=frozenset(format_body.get("format_name", "").split(",")),
        codec_name=str(video.get("codec_name", "")),
        width=int(video.get("width") or 0),
        height=int(video.get("height") or 0),
        average_fps=average_fps,
        real_fps=parse_rate(video.get("r_frame_rate")),
        duration_s=duration_s,
        size_bytes=int(format_body.get("size") or path.stat().st_size),
        has_audio=any(stream.get("codec_type") == "audio" for stream in streams),
        declared_frames=declared_frames,
    )


def probe_with_opencv(path: pathlib.Path) -> ClipMetadata:
    try:
        import cv2  # type: ignore
    except ModuleNotFoundError as exc:
        raise RuntimeError("OpenCV is required when ffprobe is unavailable") from exc
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise RuntimeError("OpenCV cannot open the video")
    try:
        fps = float(capture.get(cv2.CAP_PROP_FPS))
        frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
        fourcc = int(capture.get(cv2.CAP_PROP_FOURCC))
        codec = "".join(chr((fourcc >> (8 * index)) & 0xFF) for index in range(4))
        codec = "h264" if codec.lower() in {"avc1", "h264", "x264"} else codec
        return ClipMetadata(
            container_names=frozenset({path.suffix.lower().lstrip(".")}),
            codec_name=codec.lower(),
            width=int(capture.get(cv2.CAP_PROP_FRAME_WIDTH)),
            height=int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT)),
            average_fps=fps,
            real_fps=fps,
            duration_s=frame_count / fps if fps > 0 else 0.0,
            size_bytes=path.stat().st_size,
            has_audio=False,
            declared_frames=frame_count,
        )
    finally:
        capture.release()


def probe_clip(path: pathlib.Path) -> tuple[ClipMetadata, str]:
    if shutil.which("ffprobe"):
        return probe_with_ffprobe(path), "ffprobe"
    print(
        "notice: ffprobe unavailable; OpenCV fallback cannot verify audio "
        "streams independently",
        file=sys.stderr,
    )
    return probe_with_opencv(path), "opencv"


def decode_frame_count(path: pathlib.Path) -> int:
    try:
        import cv2  # type: ignore
    except ModuleNotFoundError as exc:
        raise RuntimeError("OpenCV is required for the decode pass") from exc
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise RuntimeError("OpenCV cannot open the video for the decode pass")
    decoded = 0
    try:
        while True:
            ok, _ = capture.read()
            if not ok:
                return decoded
            decoded += 1
    finally:
        capture.release()


def check_metadata(metadata: ClipMetadata, spec: ClipSpec) -> list[Failure]:
    failures: list[Failure] = []
    checks = [
        (
            "container",
            spec.container in metadata.container_names,
            sorted(metadata.container_names),
            spec.container,
        ),
        ("codec", metadata.codec_name == spec.codec, metadata.codec_name, spec.codec),
        (
            "resolution",
            (metadata.width, metadata.height) == (spec.width, spec.height),
            f"{metadata.width}x{metadata.height}",
            f"{spec.width}x{spec.height}",
        ),
        (
            "fps",
            abs(metadata.average_fps - spec.fps) < 0.01,
            metadata.average_fps,
            f"{spec.fps:g} constant fps",
        ),
        (
            "constant_fps",
            abs(metadata.average_fps - metadata.real_fps) < 0.01,
            f"avg={metadata.average_fps:g}, real={metadata.real_fps:g}",
            "average fps equals real fps",
        ),
        (
            "duration",
            spec.min_duration_s <= metadata.duration_s <= spec.max_duration_s,
            metadata.duration_s,
            f"{spec.min_duration_s:g}..{spec.max_duration_s:g} seconds",
        ),
        (
            "size",
            metadata.size_bytes <= spec.max_size_mb * 1_000_000,
            metadata.size_bytes,
            f"<= {spec.max_size_mb:g} MB",
        ),
        (
            "audio",
            spec.allow_audio or not metadata.has_audio,
            metadata.has_audio,
            "no audio stream",
        ),
    ]
    for attribute, passed, actual, expected in checks:
        if not passed:
            failures.append(Failure(attribute, actual, expected))
    return failures


def env_value(name: str, default: str) -> str:
    return os.getenv(name, default)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("clip", type=pathlib.Path)
    parser.add_argument(
        "--container", default=env_value("CLIP_EXPECTED_CONTAINER", "mp4")
    )
    parser.add_argument("--codec", default=env_value("CLIP_EXPECTED_CODEC", "h264"))
    parser.add_argument(
        "--width", type=int, default=int(env_value("CLIP_EXPECTED_WIDTH", "1280"))
    )
    parser.add_argument(
        "--height", type=int, default=int(env_value("CLIP_EXPECTED_HEIGHT", "720"))
    )
    parser.add_argument(
        "--fps", type=float, default=float(env_value("CLIP_EXPECTED_FPS", "20"))
    )
    parser.add_argument(
        "--min-duration",
        type=float,
        default=float(env_value("CLIP_MIN_DURATION_S", "60")),
    )
    parser.add_argument(
        "--max-duration",
        type=float,
        default=float(env_value("CLIP_MAX_DURATION_S", "120")),
    )
    parser.add_argument(
        "--max-size-mb",
        type=float,
        default=float(env_value("CLIP_MAX_SIZE_MB", "60")),
    )
    parser.add_argument(
        "--min-decode-ratio",
        type=float,
        default=float(env_value("CLIP_MIN_DECODE_RATIO", "0.99")),
    )
    parser.add_argument(
        "--allow-audio",
        action="store_true",
        default=env_value("CLIP_ALLOW_AUDIO", "false").lower() == "true",
    )
    parser.add_argument("--json", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    spec = ClipSpec(
        args.container,
        args.codec,
        args.width,
        args.height,
        args.fps,
        args.min_duration,
        args.max_duration,
        args.max_size_mb,
        args.min_decode_ratio,
        args.allow_audio,
    )
    if not args.clip.is_file():
        print(f"clip: file not found: {args.clip}", file=sys.stderr)
        return 2
    try:
        metadata, probe = probe_clip(args.clip)
        decoded = decode_frame_count(args.clip)
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as exc:
        print(f"clip probe failed: {exc}", file=sys.stderr)
        return 2
    failures = check_metadata(metadata, spec)
    required_frames = metadata.declared_frames * spec.min_decode_ratio
    if metadata.declared_frames <= 0 or decoded < required_frames:
        failures.append(
            Failure(
                "decode",
                f"{decoded}/{metadata.declared_frames}",
                f">= {spec.min_decode_ratio:.0%} of declared frames",
            )
        )
    if args.json:
        output = {
            "passed": not failures,
            "probe": probe,
            "decodedFrames": decoded,
            "declaredFrames": metadata.declared_frames,
            "failures": [failure.__dict__ for failure in failures],
        }
        print(json.dumps(output, separators=(",", ":")))
    elif failures:
        for failure in failures:
            print(f"FAIL {failure.message()}", file=sys.stderr)
    else:
        print(
            "clip spec: pass "
            f"({metadata.width}x{metadata.height}, {metadata.average_fps:g} fps, "
            f"{metadata.duration_s:.2f} s, "
            f"decoded={decoded}/{metadata.declared_frames}, probe={probe})"
        )
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
