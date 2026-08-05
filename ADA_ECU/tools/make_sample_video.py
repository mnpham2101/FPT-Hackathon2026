"""CI fixture only, never the demo source — it writes flat grey rectangles a pretrained COCO detector will not classify as ``car``, so a run against it produces zero detections and no R12 evidence.

Writes a short deterministic MP4 (mp4v) to the path given on the command line,
for the CI decoder-smoke test of the frame-source seam.
"""

import argparse


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Write a short deterministic MP4 (CI decoder-smoke fixture)."
    )
    parser.add_argument("output", help="path of the MP4 file to write")
    parser.add_argument("--width", type=int, default=320, help="frame width in pixels")
    parser.add_argument("--height", type=int, default=240, help="frame height in pixels")
    parser.add_argument("--fps", type=float, default=20.0, help="declared frames per second")
    parser.add_argument("--frames", type=int, default=40, help="number of frames to write")
    args = parser.parse_args()

    import cv2
    import numpy as np

    # Codec availability differs per OpenCV wheel/backend; try a chain before giving up.
    # All three mux into the .mp4 container the CLI names (mp4 carries mjpeg fine).
    writer = None
    tried = []
    for fourcc in ("mp4v", "avc1", "MJPG"):
        tried.append(fourcc)
        writer = cv2.VideoWriter(
            args.output, cv2.VideoWriter_fourcc(*fourcc), args.fps, (args.width, args.height)
        )
        if writer.isOpened():
            break
        writer.release()
        writer = None
    if writer is None:
        raise SystemExit(
            f"cannot open cv2.VideoWriter for output path {args.output!r} (fourccs tried: {tried})"
        )
    for i in range(args.frames):
        frame = np.full((args.height, args.width, 3), 96, dtype=np.uint8)
        # Deterministic per-frame variation: a centred grey rectangle whose shade tracks the frame ordinal.
        shade = 32 + (i * 5) % 192
        frame[args.height // 4 : 3 * args.height // 4, args.width // 4 : 3 * args.width // 4] = shade
        writer.write(frame)
    writer.release()


if __name__ == "__main__":
    main()
