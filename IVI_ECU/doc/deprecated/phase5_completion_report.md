# Phase 5 Completion Report — IVI HMI & 2D God View (AAOS)

**Branch:** `feat/phase5-ivi-hmi-dev`  
**Module:** `IVI_ECU/`  
**Requirements served:** R4 (ADA→IVI warning contract), R16 (AAOS HMI), R17 (God View)  
**Audience:** Ban giám khảo / Mentor — Milestone 1 Display Track

---

## 1. Executive Summary

- Ứng dụng IVI HMI trên **Android Automotive OS (AAOS)** đã đạt trạng thái **Architecture-Complete** và **Integration-Capable** cho Milestone 1.
- Chuỗi end-to-end đã nối: UDP R4 → `R4ListenerService` → `R4Repository` → `WarningViewModel` / `MainViewModel` → `IviWarningViewSeam` → `CanvasWarningView`.
- Debug APK ≈ **24.5 MB** — nằm trong ngân sách BTC **< 50 MB**.
- Phạm vi tùy chọn chưa làm (không chặn M1): stub 3D `SceneViewWarning3D` (`17.5.3.6`).

---

## 2. Kiến trúc hệ thống & luồng dữ liệu R4 (Task Group 5.1)

| Thành phần | Vai trò |
|---|---|
| `model/R4Message.kt` | Sealed hierarchy: `R4WarningEvent`, `R4StateMessage`, `R4ServiceError` |
| `data/R4Deserializer.kt` | Lenient JSON → `Result<R4Message>`; `warningType` lạ → `"unknown"`; field thừa bỏ qua; JSON lỗi → failure, không crash |
| `service/R4ListenerService.kt` | Foreground UDP trên `Dispatchers.IO`, cổng `BuildConfig.R4_UDP_PORT` (5004), emit `SharedFlow` |
| `data/R4Repository.kt` | Warning → edge-triggered flow; state → **last-value-wins** |
| `ui/WarningViewModel.kt` | `Idle` ↔ `Active` + auto-clear sau `WARNING_TIMEOUT_MS`; `latestScene` cho God View |

Package layout: `model` → `data` → `service` → `ui` → `di` (contract-first, không hardcode ngưỡng/port).

Harness dev: `IVI_ECU/mock-sender/` (Python UDP). Hướng dẫn cài đặt: [phase5-ivi-deploy.md](phase5-ivi-deploy.md).

---

## 3. Giao diện HMI & wake-on-warning (Task Group 5.2)

- **`MainScreen.kt` (R16):** Display Area ~70% giữa; hai side bar Home/Apps/Settings; bottom status.
- **`MainViewModel.kt` — Wake-on-Warning:**
  - Active → ép `DisplayMode.WarningView`, ghi `previousMode`
  - Idle → khôi phục `previousMode` trừ khi tài xế đã override (`userOverrodeDuringWarning`)
  - Cho phép chuyển tab thủ công trong lúc cảnh báo đang Active
- Cấu hình AAOS: `minSdk 29`, `targetSdk 33`, automotive feature, Hilt 2.58.

---

## 4. Bản đồ 2D God View Canvas (Task Group 5.3)

| Thành phần | Vai trò |
|---|---|
| `IviWarningViewSeam` | Seam render — cô lập 2D/3D; Hilt bind → `CanvasWarningView` |
| `SceneCoordinateMapper` | Pure Kotlin: mét (ego-frame) → pixel; ego neo center-bottom; **clamping** 16 px |
| `CanvasWarningView` | Ego cyan, B amber, Ghost C dashed đỏ + glow pulse theo `riskState` |
| Defensive source guard | `source != "v2x_relayed"` → `[? UNKNOWN SOURCE]` vàng + log `IVI_V2X` ERROR |
| `WarningBannerOverlay` | **Giữ file, không mount** — mentor feedback 26-07-2026: ưu tiên God View |

---

## 5. Tích hợp & độ bền hệ thống (Diagnostics)

- **Hilt:** `IviApplication`, `AppModule`, `@AndroidEntryPoint` trên `MainActivity` + `R4ListenerService`.
- **Socket lifecycle:** `datagramSocket?.close()` trong `onDestroy()` — hủy bind cổng 5004 khi service dừng.
- **UI lifecycle:** `collectAsStateWithLifecycle()` trên `currentMode` / `uiWarningState` / `latestScene` — dừng collect khi Activity STOPPED.
- Smoke tích hợp local: `FullStackIntegrationTest` (Robolectric + Hilt + UDP loopback).

---

## 6. Ma trận kiểm thử (Task Group 5.4) — GREEN

| Test file | Phủ kịch bản |
|---|---|
| `R4DeserializerTest.kt` | 5 case: warning/state hợp lệ, `future_type` → `unknown`, JSON lỗi, field thừa |
| `R4RepositoryTest.kt` | Route warning; last-value-wins state; service error không làm bẩn flow |
| `WarningViewModelTest.kt` | Idle→Active; timeout→Idle; `latestScene` set/clear |
| `MainViewModelTest.kt` | Auto-switch; auto-restore; tôn trọng user override |
| `SceneCoordinateMapperTest.kt` | Neo ego; +20 m / +5 m; clamp 500 m; null C |
| `CanvasWarningViewTest.kt` | Guard `own_sensor`; ERROR payload; risk color high |
| `FullStackIntegrationTest.kt` | UDP → service → repo → VMs → `WarningView` |

Lệnh: `./gradlew test` trong `IVI_ECU/` — toàn bộ unit/integration local **GREEN**.

---

## 7. Kế hoạch tiếp theo (Phase 6 / E2E)

1. Smoke trên Skycraft AAOS: cài APK theo [phase5-ivi-deploy.md](phase5-ivi-deploy.md) + mock-sender UDP.
2. Contract freeze với ADA/V2X: schema R4 + cổng/subnet R6 (5004) khớp blueprint CarSky.
3. Tháo mock-sender khỏi demo path; IVI tiêu thụ R4 thật từ ADA ECU — giữ nguyên seam UI, không đụng internals canvas.
4. Evidence R18/R19: pcap ADA→IVI + recording God View + assert Ghost C chỉ từ `v2x_relayed`.

---

## Tham chiếu

- Plan: [plans/phase5_tasks.md](../../plans/phase5_tasks.md)
- Requirements: [documents/Requirements/m1-cooperative-awareness.md](../../../documents/Requirements/m1-cooperative-awareness.md) (R4, R16, R17)
- Deploy smoke: [phase5-ivi-deploy.md](phase5-ivi-deploy.md)
