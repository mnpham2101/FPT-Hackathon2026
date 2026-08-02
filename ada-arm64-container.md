# ADA ARM64 Container

## Goal
Package and verify the ADA C++ runtime plus Phase 3 ML detector in one CarSky-compatible `linux/arm64` image.

## Tasks
- [x] Inspect the existing Dockerfile and deployment HLD → Verify: identify missing Python, detector, model, and video artifacts.
- [x] Update the multi-stage image to use one Python 3.11 base and install C++/ML runtime dependencies → Verify: ARM64 wheels resolved for `cv2`, `numpy`, and `onnxruntime` during the first Buildx run.
- [x] Copy detector tools, demo video, and YOLO ONNX model into the runtime image → Verify: container ML run emitted five R3 objects.
- [x] Document exact `linux/arm64` build and runtime commands → Verify: README commands match paths inside the image.
- [x] Build and execute tests with Docker Buildx → Verify: CTest passed 14/14 and both C++ mock and ML video detection ran inside the ARM64 container.

## Done When
- [x] A single-platform `linux/arm64` image builds and runs both ADA fusion and real video ML detection.

## Notes
Docker Desktop must expose a running daemon. The pretrained YOLO11n ONNX artifact is committed for offline-reproducible CI and image builds; the download helper can restore it. Local CMake caches are excluded because their host paths and architecture are not portable into the image.
