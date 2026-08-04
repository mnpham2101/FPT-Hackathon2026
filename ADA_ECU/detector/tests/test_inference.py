"""12.3.2.5 — inference.py: letterbox mapping, NMS, class filter, and the guarded ONNX session smoke test.

Runs via ``python -m pytest ADA_ECU/detector/tests`` from the repo root (the CI invocation).
The FakeDetector lives here, not in detector/ — scripted test equipment stays out of the node module (D6).
"""

import sys
from pathlib import Path

import pytest

np = pytest.importorskip("numpy")

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from inference import (  # noqa: E402
    VEHICLE_COCO_CLASSES,
    Detection,
    OnnxDetector,
    decode_output,
    letterbox,
    map_box_to_source,
    nms,
)

MODEL_PATH = Path(__file__).resolve().parents[2] / "models" / "yolo11n.onnx"


class FakeDetector:
    """Scripted stand-in satisfying the Detector protocol — returns fixed detections, no model."""

    def __init__(self, scripted: list[Detection]):
        self._scripted = scripted

    def detect(self, image) -> list[Detection]:
        return list(self._scripted)


def test_letterbox_box_maps_back_to_source_pixels():
    frame = np.zeros((720, 1280, 3), dtype=np.uint8)  # 1280x720 -> scale 0.5, pad_x 0, pad_y 140
    canvas, scale, pad_x, pad_y = letterbox(frame, size=640)

    assert canvas.shape == (640, 640, 3)
    assert scale == pytest.approx(0.5)
    assert pad_x == 0
    assert pad_y == 140
    # The pad band keeps the 114 fill; the content band took the (all-zero) frame.
    assert int(canvas[0, 0, 0]) == 114
    assert int(canvas[140, 0, 0]) == 0

    mapped = map_box_to_source((100, 240, 50, 40), scale, pad_x, pad_y, 1280, 720)
    assert mapped == pytest.approx((200.0, 200.0, 100.0, 80.0))


def test_nms_suppresses_overlapping_duplicate():
    boxes = np.array(
        [
            [100.0, 100.0, 50.0, 50.0],  # winner
            [105.0, 105.0, 50.0, 50.0],  # IoU ~0.68 with the winner -> suppressed
            [400.0, 400.0, 50.0, 50.0],  # distant -> survives
        ]
    )
    scores = np.array([0.9, 0.8, 0.7])

    keep = nms(boxes, scores, iou_threshold=0.45)

    assert keep == [0, 2]


def test_decode_output_keeps_vehicle_classes_and_drops_person():
    # One anchor per class {0 person, 2 car, 3 motorcycle, 5 bus, 7 truck}, all above threshold,
    # spatially separated so per-class NMS suppresses nothing.
    class_ids = [0, 2, 3, 5, 7]
    raw = np.zeros((1, 84, len(class_ids)), dtype=np.float32)
    for i, cls in enumerate(class_ids):
        raw[0, 0, i] = 60.0 + 120.0 * i  # cx
        raw[0, 1, i] = 300.0  # cy
        raw[0, 2, i] = 40.0  # w
        raw[0, 3, i] = 40.0  # h
        raw[0, 4 + cls, i] = 0.9

    detections = decode_output(
        raw, conf_threshold=0.25, iou_threshold=0.45,
        scale=1.0, pad_x=0, pad_y=0, frame_w=640, frame_h=640,
    )

    assert {d.coco_class for d in detections} == VEHICLE_COCO_CLASSES
    assert len(detections) == 4
    for d in detections:
        assert isinstance(d, Detection)
        assert d.score == pytest.approx(0.9)


def test_fake_detector_serves_the_detector_seam():
    scripted = [Detection(bbox_xywh=(10.0, 20.0, 30.0, 40.0), score=0.5, coco_class=2)]
    fake = FakeDetector(scripted)

    assert fake.detect(np.zeros((720, 1280, 3), dtype=np.uint8)) == scripted


def test_onnx_session_loads_model_and_runs_one_frame():
    ort = pytest.importorskip("onnxruntime")
    if not MODEL_PATH.is_file():
        pytest.skip("ADA_ECU/models/yolo11n.onnx absent — committed by a later subtask")

    session = ort.InferenceSession(str(MODEL_PATH), providers=["CPUExecutionProvider"])
    inp = session.get_inputs()[0]
    assert inp.name == "images"
    assert list(inp.shape) == [1, 3, 640, 640]
    assert list(session.get_outputs()[0].shape) == [1, 84, 8400]

    detector = OnnxDetector(str(MODEL_PATH), conf_threshold=0.25, iou_threshold=0.45)
    frame = np.full((720, 1280, 3), 114, dtype=np.uint8)  # mid-grey 1280x720 BGR frame
    result = detector.detect(frame)
    assert isinstance(result, list)
