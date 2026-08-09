# Trả lời: Blueprint 2-Node Test — Có test được không?

> **Kết luận ngắn gọn: CÓ THỂ test được.** Em đã tạo sẵn blueprint và tool rồi.

---

## 1. Ý tưởng

Thay vì phải chạy đủ 4-node như kiến trúc đầy đủ (Bench → V2X ECU → ADA ECU → IVI ECU), mình rút gọn xuống còn **2 node** để test riêng phần HMI của Task 5.1:

```
[Node 1: Mock R4 Sender]  ──eth──►  [Ethernet Bridge]  ◄──eth──  [Node 2: IVI ECU HMI]
  Container Node                                                    Skycraft AAOS Node
  Python script                                                     Kotlin/Compose App
  Bắn UDP R4 JSON                                                   Vẽ Warning View
  → 10.88.0.12:5004                                                 God-View 2D Canvas
                                                                    Ego (A), B, Ghost C
```

- **Node 1 (Mock R4 Sender)**: Đóng giả làm ADA ECU, bắn thẳng gói tin R4 JSON qua UDP đến IVI ECU theo kịch bản có sẵn.
- **Node 2 (IVI ECU HMI)**: App Kotlin AAOS nhận gói tin R4, parse JSON, hiển thị Warning View với God-View 2D Canvas.

---

## 2. Những gì đã tạo sẵn

| File | Mô tả |
|---|---|
| `requirements/blueprint-2node-task51-test.json` | Blueprint JSON, import vào Nydus là có ngay 3 nodes |
| `IVI_ECU/mock-sender/mock_r4_sender.py` | Script Python cho Node 1 — bắn sự kiện R4 |
| `IVI_ECU/mock-sender/Dockerfile` | Đóng gói Node 1 thành container image |
| `requirements/blueprint-2node-task51-test-guide.md` | Hướng dẫn từng bước setup chi tiết |

---

## 3. Kịch bản Node 1 bắn (mỗi cycle)

```
1. Xe C tiếp cận (APPROACH):  5 gói, khoảng cách 40m → 20m
                               warningType = "nlos_obstruction"
                               riskState:  low → medium → high

2. State heartbeat:            1 gói type="state" (vị trí các xe)

3. Xe C rời đi (LEAVING):     3 gói, khoảng cách 20m → 50m
                               riskState = "low"

4. Additive-version test:      1 gói warningType="future_unknown_type"
                               → IVI ECU PHẢI không crash, log WARN, hiện generic warning
```

---

## 4. Điều kiện để test được end-to-end

### ✅ Đã có (phần HMI)
- `CanvasWarningView.kt` — vẽ God-View 2D với Ego, xe B, Ghost C
- `IviWarningViewSeam.kt` — interface tách rời rendering engine
- `SceneCoordinateMapper.kt` — tính tọa độ canvas từ meter
- `WarningBannerOverlay.kt` — banner cảnh báo với countdown
- `MainScreen.kt` + `MainViewModel.kt` + `DisplayMode.kt` — khung màn hình AAOS

### ❌ Còn thiếu (Task 5.1 — tầng dữ liệu)
- `R4Message.kt` — Kotlin model nhận gói tin R4
- `R4Deserializer.kt` — parse JSON → model
- `R4ListenerService.kt` — ForegroundService mở UDP socket, nhận gói tin
- `R4Repository.kt` + `WarningViewModel.kt` — chuyển dữ liệu lên UI layer

> **Hiện tại**: Node 2 (IVI ECU) vẽ được HMI nhưng **chưa nhận được data từ UDP**. Cần hoàn thành Task 5.1 thì mới test end-to-end được.

---

## 5. Các bước để chạy thử (khi Task 5.1 code xong)

**Bước 1 — Build & push image Node 1:**
```bash
cd IVI_ECU/mock-sender
docker build -t registry.carsky.io/m1-mock-r4-sender:latest .
docker push registry.carsky.io/m1-mock-r4-sender:latest
```

**Bước 2 — Import blueprint vào Nydus:**
- Nydus → Blueprint list → **Import from File**
- Chọn file `requirements/blueprint-2node-task51-test.json`
- Sau đó **thêm ethernet pin thủ công** và **wire** trên canvas cho cả 2 node (giới hạn platform CarSky)

**Bước 3 — Build & install IVI APK:**
```bash
cd IVI_ECU && ./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

**Bước 4 — Deploy & verify:**
- Nydus → Inspector → **New Deployment** → Deploy
- Node 1 tự chạy → bắn data → IVI ECU hiển thị Warning View
- Verify bằng: `GET /api/v1/vms/{roomId}/{iviNodeKey}/screenshot`

---

## 6. Test additive-version safety

Node 1 sẽ tự gửi gói `warningType="future_unknown_type"`.  
IVI ECU phải:
- **KHÔNG crash** (không có `FATAL EXCEPTION` trong logcat)
- **Log WARN** với nội dung payload
- **Hiển thị generic warning** (không hiện đúng loại nhưng vẫn thông báo)

Verify:
```bash
adb logcat -s IVI_V2X | grep "future_unknown"
# Expect: WARN level log, no FATAL
```

---

*Tạo bởi: Task 5.1 planning — nhánh `feat/phase5-ivi-hmi-dev`*  
*Ngày: 2026-07-30*
