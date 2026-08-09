# Session Summary — 2026-07-30

## Nhánh làm việc
`feat/phase5-ivi-hmi-dev` (đã push lên GitHub)

---

## Đã hoàn thành

### Task 5.1 — R4 Data Layer (IVI ECU)

| Subtask | File | Mô tả |
|---|---|---|
| 4.5.1.1 | `model/R4Message.kt` | Sealed class `R4WarningEvent` \| `R4StateMessage` với kotlinx.serialization |
| 4.5.1.1 | `model/SceneGeometry.kt` | Fix thiếu `@Serializable` |
| 4.5.1.2 | `data/R4Deserializer.kt` | Parse JSON chịu lỗi, unknown `warningType` → degrade gracefully |
| 4.5.1.3 | `service/R4ListenerService.kt` | ForegroundService nhận UDP port 5004, auto-reconnect 5 lần |
| 4.5.1.4 | `data/R4Repository.kt` | Route event/state qua SharedFlow/StateFlow |
| 4.5.1.4 | `ui/WarningViewModel.kt` + `WarningUiState.kt` | Idle→Active→Idle tự clear sau 10s |

**Unit tests:** 9 tests (5 cho Deserializer + 4 cho ViewModel) — pass local

---

### Blueprint 2-Node Test

Tạo blueprint tối giản để test HMI **không cần** ADA ECU và V2X ECU thật:

```
[Mock R4 Sender] → [Ethernet Bridge] ← [IVI ECU HMI]
 Container Node       (CarSky)          Skycraft AAOS
 Bắn UDP R4 JSON                        Vẽ Warning View
```

| File | Mô tả |
|---|---|
| `requirements/blueprint-2node-task51-test.json` | Import vào Nydus |
| `IVI_ECU/mock-sender/mock_r4_sender.py` | Script Python bắn sự kiện R4 |
| `IVI_ECU/mock-sender/Dockerfile` | Container image cho Node 1 |

---

## Còn lại để test end-to-end

- [ ] Build & push Docker image: `docker build + push registry.carsky.io/m1-mock-r4-sender:latest`
- [ ] Import blueprint vào Nydus, add ethernet pin thủ công, deploy
- [ ] Install IVI APK lên AAOS node qua ADB

---

## Kiến trúc tầng dữ liệu sau Task 5.1

```
UDP :5004
  ↓
R4ListenerService  →  R4Repository  →  WarningViewModel  →  CanvasWarningView (HMI)
  (ForegroundSvc)      (SharedFlow)      (StateFlow UI)        (đã có từ trước)
```
