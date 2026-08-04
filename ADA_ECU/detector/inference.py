"""YOLO11n ONNX Runtime CPU inference behind the swappable Detector protocol (HLD D6, report §3(g)).

Model I/O contract (Ultralytics YOLO11n ONNX export, input 640):
input ``images`` float32 (1,3,640,640), RGB, values 0..1; output (1,84,8400) —
rows 0-3 = cx,cy,w,h in 640-letterbox pixels, rows 4-83 = 80 per-class scores (already sigmoid).
Frames arrive as BGR uint8 HxWx3 numpy arrays (OpenCV convention).

The COCO→R3 class collapse to the string "vehicle" happens downstream (emit);
this module returns ``Detection`` with the raw COCO class id.
"""

from dataclasses import dataclass
from typing import Protocol

import numpy as np
import onnxruntime as ort

# COCO classes kept by the detector: car=2, motorcycle=3, bus=5, truck=7.
VEHICLE_COCO_CLASSES = {2, 3, 5, 7}

INPUT_SIZE = 640
PAD_VALUE = 114


@dataclass
class Detection:
    bbox_xywh: tuple[float, float, float, float]  # SOURCE-pixel coords, x/y top-left
    score: float
    coco_class: int


class Detector(Protocol):
    def detect(self, image) -> list[Detection]: ...


def letterbox(image: np.ndarray, size: int = INPUT_SIZE) -> tuple[np.ndarray, float, int, int]:
    """Resize ``image`` into a ``size`` x ``size`` canvas preserving aspect ratio.

    Returns ``(canvas, scale, pad_x, pad_y)`` where ``scale = min(size/w, size/h)`` and
    ``pad_x``/``pad_y`` are the left/top pad widths around the resized content.
    """
    h, w = image.shape[:2]
    scale = min(size / w, size / h)
    new_w = int(round(w * scale))
    new_h = int(round(h * scale))
    # cv2 is barred from this module by the layer rule; NN resize costs a little accuracy, not correctness.
    rows = np.round(np.linspace(0, h - 1, new_h)).astype(int)
    cols = np.round(np.linspace(0, w - 1, new_w)).astype(int)
    resized = image[rows][:, cols]
    canvas = np.full((size, size, image.shape[2]), PAD_VALUE, dtype=image.dtype)
    pad_x = (size - new_w) // 2
    pad_y = (size - new_h) // 2
    canvas[pad_y : pad_y + new_h, pad_x : pad_x + new_w] = resized
    return canvas, scale, pad_x, pad_y


def map_box_to_source(
    bbox_xywh_letterbox: tuple[float, float, float, float],
    scale: float,
    pad_x: float,
    pad_y: float,
    frame_w: int,
    frame_h: int,
) -> tuple[float, float, float, float]:
    """Map a top-left x,y,w,h box from 640-letterbox pixels back to source pixels, clipped to the frame."""
    x, y, w, h = bbox_xywh_letterbox
    x1 = (x - pad_x) / scale
    y1 = (y - pad_y) / scale
    x2 = (x + w - pad_x) / scale
    y2 = (y + h - pad_y) / scale
    x1 = min(max(x1, 0.0), float(frame_w))
    y1 = min(max(y1, 0.0), float(frame_h))
    x2 = min(max(x2, 0.0), float(frame_w))
    y2 = min(max(y2, 0.0), float(frame_h))
    return (x1, y1, x2 - x1, y2 - y1)


def nms(boxes_xywh: np.ndarray, scores: np.ndarray, iou_threshold: float) -> list[int]:
    """Greedy non-maximum suppression over top-left x,y,w,h boxes; returns kept indices, best score first."""
    boxes = np.asarray(boxes_xywh, dtype=np.float64).reshape(-1, 4)
    scores = np.asarray(scores, dtype=np.float64).reshape(-1)
    if boxes.shape[0] == 0:
        return []
    x1 = boxes[:, 0]
    y1 = boxes[:, 1]
    x2 = boxes[:, 0] + boxes[:, 2]
    y2 = boxes[:, 1] + boxes[:, 3]
    areas = boxes[:, 2] * boxes[:, 3]
    order = np.argsort(scores)[::-1]
    keep: list[int] = []
    while order.size > 0:
        i = order[0]
        keep.append(int(i))
        rest = order[1:]
        xx1 = np.maximum(x1[i], x1[rest])
        yy1 = np.maximum(y1[i], y1[rest])
        xx2 = np.minimum(x2[i], x2[rest])
        yy2 = np.minimum(y2[i], y2[rest])
        inter = np.maximum(0.0, xx2 - xx1) * np.maximum(0.0, yy2 - yy1)
        union = areas[i] + areas[rest] - inter
        iou = np.where(union > 0.0, inter / union, 0.0)
        order = rest[iou <= iou_threshold]
    return keep


def decode_output(
    raw: np.ndarray,
    conf_threshold: float,
    iou_threshold: float,
    scale: float,
    pad_x: float,
    pad_y: float,
    frame_w: int,
    frame_h: int,
) -> list[Detection]:
    """Decode a raw (1,84,N) model output into vehicle ``Detection``s in source pixels."""
    preds = np.asarray(raw, dtype=np.float32)
    if preds.ndim == 3:
        preds = preds[0]  # (84, N)
    class_scores = preds[4:84]  # (80, N)
    classes = np.argmax(class_scores, axis=0)
    scores = class_scores[classes, np.arange(class_scores.shape[1])]

    # Confidence gate, then the vehicle-class keep; per-class NMS makes filtering before NMS
    # result-identical to filtering after it.
    mask = (scores >= conf_threshold) & np.isin(classes, list(VEHICLE_COCO_CLASSES))
    if not np.any(mask):
        return []
    cx, cy, w, h = (preds[k, mask] for k in range(4))
    classes = classes[mask]
    scores = scores[mask]

    # cx,cy,w,h (letterbox pixels) -> top-left x,y,w,h, then back to source pixels.
    boxes_src = np.array(
        [
            map_box_to_source(
                (cx[j] - w[j] / 2.0, cy[j] - h[j] / 2.0, w[j], h[j]),
                scale, pad_x, pad_y, frame_w, frame_h,
            )
            for j in range(classes.size)
        ],
        dtype=np.float64,
    )

    detections: list[Detection] = []
    for cls in np.unique(classes):
        idx = np.where(classes == cls)[0]
        for k in nms(boxes_src[idx], scores[idx], iou_threshold):
            j = idx[k]
            detections.append(
                Detection(
                    bbox_xywh=tuple(float(v) for v in boxes_src[j]),
                    score=float(scores[j]),
                    coco_class=int(cls),
                )
            )
    return detections


class OnnxDetector:
    """The YOLO11n session over ONNX Runtime's CPU provider — the real Detector implementation (D6)."""

    def __init__(self, model_path: str, conf_threshold: float, iou_threshold: float):
        self._conf_threshold = conf_threshold
        self._iou_threshold = iou_threshold
        self._session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        self._input_name = self._session.get_inputs()[0].name

    def detect(self, image) -> list[Detection]:
        frame_h, frame_w = image.shape[:2]
        canvas, scale, pad_x, pad_y = letterbox(image, INPUT_SIZE)
        rgb = canvas[:, :, ::-1]  # BGR -> RGB
        blob = rgb.astype(np.float32) / 255.0
        blob = np.ascontiguousarray(np.transpose(blob, (2, 0, 1))[np.newaxis])
        raw = self._session.run(None, {self._input_name: blob})[0]
        return decode_output(
            raw,
            self._conf_threshold,
            self._iou_threshold,
            scale,
            pad_x,
            pad_y,
            frame_w,
            frame_h,
        )
