"""Stable ``own:<n>`` ids across sampled frames by greedy IoU matching against the previous
sampled frame — no tracking library, no time-based prediction (D6,
ADA_ECU/doc/ada-ecu-design-decisions.md)."""

from typing import Optional

Box = tuple[float, float, float, float]  # (x, y, w, h), x/y top-left, source pixels


def _iou(a: Box, b: Box) -> float:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    ix = max(0.0, min(ax + aw, bx + bw) - max(ax, bx))
    iy = max(0.0, min(ay + ah, by + bh) - max(ay, by))
    inter = ix * iy
    union = aw * ah + bw * bh - inter
    if union <= 0:
        return 0.0
    return inter / union


class Tracker:
    """Greedy IoU matcher: hold the previous frame's id while IoU >= iou_min, else mint a fresh id."""

    def __init__(self, iou_min: float):
        self._iou_min = iou_min
        self._prev: list[tuple[Box, str]] = []  # previous SAMPLED frame's (box, id)
        self._next_n = 0  # monotonic; ids are never reused

    def assign(self, boxes: list[Box]) -> list[str]:
        """boxes are (x, y, w, h) in source pixels, x/y top-left; returns ids aligned with input order."""
        pairs = [
            (_iou(box, pbox), ci, pi)
            for ci, box in enumerate(boxes)
            for pi, (pbox, _) in enumerate(self._prev)
        ]
        pairs.sort(key=lambda t: t[0], reverse=True)

        ids: list[Optional[str]] = [None] * len(boxes)
        used_prev: set[int] = set()
        for iou, ci, pi in pairs:
            if iou < self._iou_min:
                break  # sorted descending: nothing later can match either
            if ids[ci] is not None or pi in used_prev:
                continue
            ids[ci] = self._prev[pi][1]
            used_prev.add(pi)

        for ci in range(len(boxes)):
            if ids[ci] is None:
                # The detector mints only own:<n> — it can never mint a v2x: id, the
                # structural half of the zero-C-detections argument (D6).
                ids[ci] = f"own:{self._next_n}"
                self._next_n += 1

        self._prev = list(zip(boxes, ids))
        return ids  # type: ignore[return-value]  # every slot filled above
