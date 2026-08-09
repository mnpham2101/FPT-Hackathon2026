# Blueprint 2-Node Task 5.1 Test — Setup Guide

Blueprint tối giản để test Task 5.1 (R4 Data Layer). Thay vì dùng đủ 4 nodes + ADA ECU + V2X ECU, blueprint này bỏ qua tất cả, chỉ cần:

- **Node 1: Mock R4 Sender** — Container Node, bắn gói UDP R4 JSON trực tiếp đến IVI ECU
- **Node 2: IVI ECU** — Skycraft AAOS Node, chạy app Kotlin + vẽ Warning View HMI

```
[Mock R4 Sender] ──eth──► [Ethernet Bridge] ◄──eth── [IVI ECU]
  10.88.0.11                                           10.88.0.12
  port 5004 (sender)                           port 5004 (listener)
```

## Bước 1: Build & Push Docker image cho Mock Sender

```bash
cd IVI_ECU/mock-sender
docker build -t registry.carsky.io/m1-mock-r4-sender:latest .
docker push registry.carsky.io/m1-mock-r4-sender:latest
```

## Bước 2: Import blueprint vào Nydus

1. Nydus → Blueprint list → **Import from File**
2. Chọn file `requirements/blueprint-2node-task51-test.json`
3. Blueprint sẽ tạo 3 nodes (Mock Sender, Bridge, IVI ECU) — **KHÔNG có ethernet pins** (giới hạn của platform)

## Bước 3: Thêm Ethernet pins thủ công trên Nydus UI Canvas

Đây là bước **BẮT BUỘC** vì REST API không hỗ trợ tạo ETHERNET pin.

1. Mở blueprint trên Nydus canvas
2. **Mock R4 Sender node**: Inspector → Add Pin → type `ETHERNET` → address `10.88.0.11`
3. **IVI ECU node**: Inspector → Add Pin → type `ETHERNET` → address `10.88.0.12`
4. **Wire**: Kéo từ pin eth của Mock Sender → Ethernet Bridge. Kéo từ pin eth của IVI ECU → Ethernet Bridge.

## Bước 4: Build & Install IVI APK lên AAOS

```bash
cd IVI_ECU
./gradlew assembleDebug
# Sau khi AAOS node chạy, install APK qua ADB tunnel của CarSky
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Bước 5: Deploy và Verify

1. Nydus → Inspector (empty canvas) → **New Deployment** → chọn Device → **Deploy**
2. Chờ `2/3` nodes Running (Bridge không tính)
3. Mock Sender sẽ tự động chạy → IVI ECU sẽ nhận packets → Warning View hiện lên

## Verify từng bước

```bash
# Check logs của Mock Sender
GET /api/v1/deployments/{roomId}/logs/{mockSenderNodeKey}

# Screenshot IVI ECU để xem HMI
GET /api/v1/vms/{roomId}/{iviNodeKey}/screenshot

# ADB shell vào IVI ECU để xem logcat
POST /api/v1/vms/{roomId}/{iviNodeKey}/shell
# body: {"command": "adb logcat -s IVI_V2X"}
```

## Test additive-version (warningType không biết)

Mock Sender sẽ tự động gửi 1 packet `warningType=future_unknown_type` mỗi cycle.
IVI ECU **phải không crash**, log WARN và hiển thị generic warning.
Verify bằng logcat: tìm `WARN` với `future_unknown_type` — **không có** `FATAL EXCEPTION`.

## Teardown

Nydus → Deployment Viewer → **Delete Deployment**
