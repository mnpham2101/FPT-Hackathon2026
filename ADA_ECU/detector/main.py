"""Detector composition root (R12, HLD §6) — argv parsing, wiring, exit codes.

Controller only: every rule lives in the sibling modules; this file connects
source → pacer → inference → distance → tracker → emit and owns the process
boundary. stdout carries only R3 JSONL (the emitter writes it); every startup
line, progress note and fatal error goes to stderr — the reader parses every
stdout line, so a stray print there corrupts the process contract.

Exit codes:
    0 — clean EOF of the frame source, or SIGTERM/SIGINT received (graceful stop).
    1 — invalid configuration; the ValueError message naming the env var is on stderr.
    2 — startup failure: missing/unopenable clip or unloadable model, path named on stderr.

Runs as a script (``python3 /app/detector/main.py`` or
``python ADA_ECU/detector/main.py`` from the repo root): the interpreter puts the
script's directory on sys.path, so the flat sibling imports below resolve.
"""

import argparse
import signal
import sys
import time

from config import DetectorConfig, load_config
from distance import estimate_range, lateral_offset_m
from emit import EmitItem, R3Emitter
from frame_source import FileFrameSource, SyntheticFrameSource
from inference import OnnxDetector
from pacer import pace
from tracker import Tracker


class _Terminated(BaseException):
    """Raised from the signal handler to leave the frame loop promptly.

    BaseException on purpose, mirroring KeyboardInterrupt: the startup blocks
    catch ``Exception`` to map real failures onto exit code 2, and a SIGTERM
    landing during construction must not be re-labelled a startup failure.
    """


# Module-level stop flag: belt to the handler's raised-exception braces.
_stop_requested = False


def _handle_signal(signum, frame):
    global _stop_requested
    _stop_requested = True
    raise _Terminated()


def _install_signal_handlers() -> None:
    # Cross-platform harmless: SIGTERM exists on Windows but delivery differs
    # (TerminateProcess never runs handlers); SIGINT works everywhere.
    for name in ("SIGTERM", "SIGINT"):
        sig = getattr(signal, name, None)
        if sig is None:
            continue
        try:
            signal.signal(sig, _handle_signal)
        except (ValueError, OSError):
            pass  # not the main thread, or the platform refuses: run without it


def run(config: DetectorConfig, source, detector, tracker, emitter, stream) -> int:
    """Drive the HLD §6 pipeline: source → pacer → inference → distance → tracker → emit.

    The pacer wraps the source unconditionally; behaviour is decided by config
    alone (``enabled=config.realtime_pacing``). Returns 0 on clean EOF.
    """
    frames = pace(
        source.iter_frames(),
        fps=config.clip_fps,
        stride=config.frame_stride,
        start_delay_s=config.start_delay_s,
        enabled=config.realtime_pacing,
    )
    for frame in frames:
        if _stop_requested:
            return 0
        # CLOCK_REALTIME at frame capture — taken when the frame leaves the pacer.
        measured_ms = int(time.time() * 1000)
        dets = detector.detect(frame.image)
        ids = tracker.assign([d.bbox_xywh for d in dets])
        items = []
        for det, track_id in zip(dets, ids):
            x, y, w, h = det.bbox_xywh
            distance = estimate_range(
                w, frame.width, config.camera_hfov_deg, config.vehicle_width_m
            )
            if distance is None:
                continue  # degenerate box: no range, the detection is dropped
            lateral = lateral_offset_m(
                x + w / 2.0, frame.width, distance, config.camera_hfov_deg
            )
            items.append(
                EmitItem(
                    track_id=track_id,
                    distance_m=distance,
                    lateral_m=lateral,
                    confidence=det.score,
                    clip_ts_ms=frame.timestamp_ms,
                )
            )
        # One line per detection per sampled frame; an empty frame emits nothing.
        if items:
            emitter.emit_frame(items, measured_ms, stream)
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="ADA detector subprocess: R3 JSONL on stdout, one line per detection."
    )
    parser.add_argument(
        "--synthetic",
        type=int,
        default=None,
        metavar="N",
        help="use a SyntheticFrameSource of N frames instead of the clip "
        "(the CI import-and-plumbing smoke)",
    )
    args = parser.parse_args(argv)

    try:
        config = load_config()
    except ValueError as exc:
        print(f"detector: invalid config: {exc}", file=sys.stderr)
        return 1

    _install_signal_handlers()

    # Everything after handler installation sits under one graceful-termination
    # catch: a SIGTERM is a clean exit 0 wherever it lands - during source or
    # model construction, the startup print, or the frame loop. A narrower try
    # around run() alone leaves windows in which the raised _Terminated escapes
    # as an unhandled traceback (exit 1), which is what the CI SIGTERM test
    # caught on Linux.
    try:
        # Construction order: source before model, so a missing clip fails before
        # (and independently of) a missing model.
        try:
            if args.synthetic is not None:
                # Synthetic frames are pre-strided; the pacer still gets the config
                # stride/fps — fidelity does not matter on this smoke path.
                source = SyntheticFrameSource(count=args.synthetic)
                source_desc = f"synthetic source ({args.synthetic} frames)"
            else:
                source = FileFrameSource(
                    config.video_clip_path, config.frame_stride, config.loop
                )
                source_desc = f"file source (clip {config.video_clip_path})"
        except Exception as exc:
            print(f"detector: startup failure: {exc}", file=sys.stderr)
            return 2

        try:
            detector = OnnxDetector(
                config.model_path, config.conf_threshold, config.iou_threshold
            )
        except Exception as exc:
            print(
                f"detector: startup failure: cannot load model {config.model_path}: {exc}",
                file=sys.stderr,
            )
            return 2

        tracker = Tracker(config.track_iou_min)
        emitter = R3Emitter()

        print(f"detector: starting: {source_desc}; model {config.model_path}", file=sys.stderr)
        sys.stderr.flush()

        return run(config, source, detector, tracker, emitter, sys.stdout)
    except (_Terminated, KeyboardInterrupt):
        return 0


if __name__ == "__main__":
    sys.exit(main())
