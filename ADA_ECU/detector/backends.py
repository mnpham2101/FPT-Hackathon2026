"""Deterministic and YOLO ONNX vehicle detector backends."""

from __future__ import annotations

import json
import os
import sys
from collections.abc import Iterable
from typing import ClassVar

from detector.association import SingleVehicleBAssociation
from detector.distance import PinholeProjector, RangeRateEstimator
from detector.emission import r3_vehicle_b
from detector.models import Detection, FrameInput


class PlaceholderVehicleBackend:
    """Deterministic B detection used by CI and smoke tests."""

    def detect(self, frame: FrameInput) -> Iterable[dict]:
        yield r3_vehicle_b(
            distance_m=12.0 + frame.frame_index * 0.05,
            lateral_m=0.0,
            confidence=0.5,
            timestamp_ms=frame.timestamp_ms,
            speed_mps=0.0,
        )


class YoloOnnxVehicleBackend:
    """YOLO11 ONNX vehicle detector that emits the ego-lane occluder as B."""

    vehicle_class_ids: ClassVar[set[int]] = {2, 3, 5, 7}
    coco_names: ClassVar[dict[int, str]] = {
        2: "car",
        3: "motorcycle",
        5: "bus",
        7: "truck",
    }

    def __init__(
        self,
        model_path: str,
        confidence_threshold: float,
        iou_threshold: float,
        input_size: int,
        vehicle_width_m: float,
        focal_px: float,
        log_detections: bool,
    ) -> None:
        if not model_path:
            raise RuntimeError("--model is required when --backend yolo-onnx")
        if not os.path.exists(model_path):
            raise RuntimeError(f"YOLO ONNX model not found: {model_path}")
        try:
            import numpy as np  # type: ignore
            import onnxruntime as ort  # type: ignore
        except ModuleNotFoundError as exc:
            raise RuntimeError(
                "YOLO backend dependencies are missing. Install with: "
                "python3 -m pip install -r ADA_ECU/requirements.txt"
            ) from exc
        self.np = np
        self.session = ort.InferenceSession(
            model_path, providers=["CPUExecutionProvider"]
        )
        self.input_name = self.session.get_inputs()[0].name
        shape = self.session.get_inputs()[0].shape
        if len(shape) == 4 and isinstance(shape[2], int) and isinstance(shape[3], int):
            self.input_size = int(shape[2])
        else:
            self.input_size = input_size
        self.confidence_threshold = confidence_threshold
        self.iou_threshold = iou_threshold
        self.log_detections = log_detections
        self.association = SingleVehicleBAssociation()
        self.projector = PinholeProjector(vehicle_width_m, focal_px)
        self.range_rate = RangeRateEstimator()

    def detect(self, frame: FrameInput) -> Iterable[dict]:
        if frame.image is None:
            return []
        tensor, scale, pad_x, pad_y = self._preprocess(frame.image)
        outputs = self.session.run(None, {self.input_name: tensor})
        detections = self._postprocess(
            outputs[0], frame.width, frame.height, scale, pad_x, pad_y
        )
        vehicle_b = self.association.select(detections, frame.width)
        if vehicle_b is None:
            return []
        distance_m, lateral_m = self.projector.project(
            vehicle_b.width, vehicle_b.center_x, frame.width
        )
        confidence = round(vehicle_b.score, 3)
        speed_mps = self.range_rate.update(distance_m, frame.timestamp_ms)
        if self.log_detections:
            self._log_detection(frame, vehicle_b, confidence, distance_m)
        return [
            r3_vehicle_b(
                distance_m,
                lateral_m,
                confidence,
                frame.timestamp_ms,
                speed_mps,
            )
        ]

    @staticmethod
    def _log_detection(
        frame: FrameInput,
        vehicle_b: Detection,
        confidence: float,
        distance_m: float,
    ) -> None:
        event = {
            "event": "ml_detection",
            "frame": frame.frame_index,
            "timestampMs": frame.timestamp_ms,
            "class": vehicle_b.class_name,
            "confidence": confidence,
            "bbox": [
                round(vehicle_b.x1, 1),
                round(vehicle_b.y1, 1),
                round(vehicle_b.x2, 1),
                round(vehicle_b.y2, 1),
            ],
            "distance": round(distance_m, 3),
        }
        print(json.dumps(event, separators=(",", ":")), file=sys.stderr, flush=True)

    def _preprocess(self, image: object) -> tuple[object, float, float, float]:
        import cv2  # type: ignore

        np = self.np
        original_h, original_w = image.shape[:2]
        scale = min(self.input_size / original_w, self.input_size / original_h)
        resized_w = round(original_w * scale)
        resized_h = round(original_h * scale)
        resized = cv2.resize(
            image, (resized_w, resized_h), interpolation=cv2.INTER_LINEAR
        )
        pad_x = (self.input_size - resized_w) / 2.0
        pad_y = (self.input_size - resized_h) / 2.0
        left = round(pad_x)
        top = round(pad_y)
        canvas = np.full((self.input_size, self.input_size, 3), 114, dtype=np.uint8)
        canvas[top : top + resized_h, left : left + resized_w] = resized
        rgb = cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB)
        tensor = rgb.astype(np.float32) / 255.0
        tensor = np.transpose(tensor, (2, 0, 1))[None, ...]
        return tensor, scale, float(left), float(top)

    def _postprocess(
        self,
        output: object,
        width: int,
        height: int,
        scale: float,
        pad_x: float,
        pad_y: float,
    ) -> list[Detection]:
        np = self.np
        predictions = np.squeeze(output)
        if predictions.ndim != 2:
            return []
        if predictions.shape[0] < predictions.shape[1] and predictions.shape[0] <= 256:
            predictions = predictions.T
        candidates: list[Detection] = []
        for row in predictions:
            detection = self._decode_row(row, width, height, scale, pad_x, pad_y)
            if detection is not None:
                candidates.append(detection)
        return self._nms(candidates)

    def _decode_row(
        self,
        row: object,
        width: int,
        height: int,
        scale: float,
        pad_x: float,
        pad_y: float,
    ) -> Detection | None:
        np = self.np
        if row.shape[0] < 6:
            return None
        class_scores = row[4:]
        class_id = int(np.argmax(class_scores))
        score = float(class_scores[class_id])
        if class_id not in self.vehicle_class_ids or score < self.confidence_threshold:
            return None
        center_x, center_y, box_w, box_h = [float(value) for value in row[:4]]
        x1 = min(max((center_x - box_w / 2 - pad_x) / scale, 0.0), float(width))
        y1 = min(max((center_y - box_h / 2 - pad_y) / scale, 0.0), float(height))
        x2 = min(max((center_x + box_w / 2 - pad_x) / scale, 0.0), float(width))
        y2 = min(max((center_y + box_h / 2 - pad_y) / scale, 0.0), float(height))
        if x2 <= x1 or y2 <= y1:
            return None
        return Detection(class_id, self.coco_names[class_id], score, x1, y1, x2, y2)

    def _nms(self, detections: list[Detection]) -> list[Detection]:
        selected: list[Detection] = []
        remaining = sorted(detections, key=lambda item: item.score, reverse=True)
        while remaining:
            current = remaining.pop(0)
            selected.append(current)
            remaining = [
                item
                for item in remaining
                if self._iou(current, item) < self.iou_threshold
            ]
        return selected

    @staticmethod
    def _iou(a: Detection, b: Detection) -> float:
        inter_x1 = max(a.x1, b.x1)
        inter_y1 = max(a.y1, b.y1)
        inter_x2 = min(a.x2, b.x2)
        inter_y2 = min(a.y2, b.y2)
        inter = max(0.0, inter_x2 - inter_x1) * max(0.0, inter_y2 - inter_y1)
        union = a.area + b.area - inter
        return inter / union if union > 0 else 0.0
