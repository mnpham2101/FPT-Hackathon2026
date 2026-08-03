"""Association policy for the single visible occluder required by M1."""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass

from detector.models import Detection


@dataclass(frozen=True)
class SingleVehicleBAssociation:
    """Select one dominant ego-lane vehicle and keep the stable ID ``own:B``.

    M1 intentionally tracks only the visible occluding vehicle B. This policy
    does not claim multi-object identity matching across frames; it maps the
    best current ego-lane candidate to the one stable scenario identity.
    """

    track_id: str = "own:B"

    def select(
        self, detections: Sequence[Detection], frame_width: int
    ) -> Detection | None:
        if not detections:
            return None
        plausible = [
            item
            for item in detections
            if item.width <= frame_width * 0.80
            and item.height > 0
            and item.width / item.height <= 3.5
        ]
        candidates = plausible or detections
        half_width = max(frame_width / 2.0, 1.0)
        return max(
            candidates,
            key=lambda item: (
                item.area
                * max(0.05, 1.0 - abs(item.center_x - half_width) / half_width),
                item.score,
            ),
        )
