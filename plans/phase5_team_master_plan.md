# Phase 5 — IVI HMI Team Master Plan

**Cập nhật:** 03/08/2026  
**Nhánh Vinh (Dev):** `feat/phase5-ivi-hmi-dev`  
**Nhánh bạn đồng đội (Complete):** `feat/phase5-ivi-hmi-complete`  
**Target merge:** `main` trước 10/08/2026

> **Nhận xét anh Minh (03/08/2026):** Hai đứa nên work together theo task plan chung, chia việc song song. Thay vì code đè lên nhau, nên chia ra: (1) Testing, (2) Deploy & hiển thị GUI, (3) Simulate bản tin ADA. Cần có Presentation + Working Demo + Report kỹ thuật tường minh.

---

## 📊 Tình trạng hiện tại (Gap Analysis)

### Đã làm xong ✅ (từ cả 2 nhánh)

| Thành phần | Nhánh | Tình trạng |
|---|---|---|
| `R4Deserializer` — parse JSON → Kotlin model | cả hai | ✅ Done |
| `R4ListenerService` — UDP foreground service, SharedFlow | cả hai | ✅ Done |
| `R4Repository` — edge-triggered flow + last-value-wins state | complete | ✅ Done |
| `WarningViewModel` — Idle ↔ Active, auto-timeout, `latestScene` | cả hai | ✅ Done (dev có fix §4.1 R19 guard) |
| `MainViewModel` — wake-on-warning auto-switch + user override | complete | ✅ Done |
| `CanvasWarningView` — God-View 2D: Ego, B, Ghost C glow/badge/guard | cả hai | ✅ Done |
| `SceneCoordinateMapper` — mét → pixel, clamping | complete | ✅ Done |
| Hilt DI — `IviApplication`, `AppModule`, `@AndroidEntryPoint` | complete | ✅ Done |
| Mock sender Python — `mock_r4_sender.py` | complete | ✅ Done (cần fix schema §4.4 MOCK-1) |
| Unit tests — Deserializer, Repository, VMs, Mapper | cả hai | ✅ Done |
| `FullStackIntegrationTest` (Robolectric + Hilt + UDP) | complete | ✅ Done |
| CI fix — `verify-arm64-image` fallback pull by tag | dev | ✅ Done |
| APK build guide — `apk-deploy.md` | complete | ✅ Done |

### Còn thiếu ❌ (cần làm ngay — chặn Demo)

| # | Việc còn thiếu | Ưu tiên | Giao cho |
|---|---|---|---|
| **UI-1** | `HomeView` thực sự (Automotive Dashboard: clock, speed, V2X link) thay placeholder text | 🔴 P1 | **Vinh** |
| **UI-3** | `CanvasWarningView` đã có nhưng `MainScreen` vẫn render `WarningViewPlaceholder()` ở 1 path | 🔴 P1 | **Bạn đồng đội** |
| **MERGE** | Merge fix §4.1 (R19 guard), §4.2 (datagram truncation), §4.3 (R4ServiceError), §4.4 (warningType) từ nhánh dev → complete | 🔴 P1 | **Vinh** |
| **MOCK-1** | Fix mock sender schema: `object.class` field bị thiếu, `vehicles` key sai (§4.4 MOCK-1) | 🟡 P2 | **Bạn đồng đội** |
| **DEPLOY** | ADB tunnel + install APK + smoke test trên CarSky AAOS node thật | 🔴 P1 | **Cả hai** |
| **PRES-1** | Slide deck Phase 5 (`presentation/phase5/`) — working demo video + kỹ thuật | 🟡 P2 | **Bạn đồng đội** |
| **WIKI-1** | Technical Deep-Dive Wiki (`documents/Design/MODULE-DESIGN/IVI-ECU/ivi-technical-wiki.md`) | 🟡 P2 | **Vinh** |

---

## 👥 Phân công Song Song

### 🔵 VINH — `feat/phase5-ivi-hmi-dev`

**Mục tiêu chính: Merge fixes + HomeView UI + Wiki kỹ thuật**

#### Task V-1: Merge 4 bug fixes từ nhánh dev → complete branch (30 phút)
```
git checkout feat/phase5-ivi-hmi-complete
git merge feat/phase5-ivi-hmi-dev --no-ff -m "fix: merge §4.1-§4.4 defects from dev into complete"
```
Các fix cần merge:
- `7c48b1b` — §4.2 DatagramPacket truncation fix
- `8203714` — §4.1 R19 provenance guard fix + regression test
- `3cdbc6b` — §4.4 warningType wire value preserved (không rewrite)
- `6542211` — §4.3 R4ServiceError out of wire hierarchy

#### Task V-2: Xây dựng `HomeView` Automotive Dashboard (2–3 giờ)

**File:** `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/HomeScreen.kt` (NEW)

Thiết kế:
```
┌─────────────────────────────────────┐
│  🕐  23:04:15       V2X: ACTIVE     │  ← Top bar: clock + V2X status
│                                     │
│    ┌──────────────────────────────┐ │
│    │   🚗  IVI Dashboard          │ │  ← Central dark panel
│    │                              │ │
│    │   Speed    Heading  Mode     │ │
│    │   ─────    ───────  ────     │ │
│    │   0 km/h      N     P        │ │
│    │                              │ │
│    │   ────────────────────────   │ │
│    │   System Status              │ │
│    │   R4 UDP: Listening :5004    │ │
│    │   Last packet: N/A           │ │
│    │   Warnings: 0                │ │
│    └──────────────────────────────┘ │
│                                     │
│  IVI · R16 · AAOS · Phase 5       │  ← Bottom caption
└─────────────────────────────────────┘
```

Yêu cầu:
- Dark automotive theme (BackgroundColor `#1A1A2E`, AccentColor `#00D4FF`)
- Clock cập nhật real-time mỗi giây (LaunchedEffect + delay(1000))
- V2X Link status: `ACTIVE` (xanh) / `ERROR` (đỏ) / `STANDBY` (xám)
- Responsive — không hardcode kích thước pixel

Mount vào `DisplayModeSwitcher` trong `MainScreen.kt`:
```kotlin
DisplayMode.HomeView -> HomeScreen(linkStatus = v2xLinkStatus)
```

#### Task V-3: Technical Deep-Dive Wiki (2–3 giờ)

**File:** `documents/Design/MODULE-DESIGN/IVI-ECU/ivi-technical-wiki.md` (NEW)

Trả lời các câu hỏi anh Minh đặt ra (dùng AI prompt):
1. **AAOS là gì?** Tại sao ô tô cần AAOS thay vì Android điện thoại?
2. **Pipeline R4:** `byte[]` UDP → `String` JSON → `R4WarningEvent` Kotlin — vẽ sequence diagram
3. **MVVM trong IVI:** Service → Repository → ViewModel (StateFlow) → Compose UI
4. **UDP vs SOME/IP vs gRPC:** Tại sao chọn UDP raw cho V2X cảnh báo độ trễ thấp?
5. **Compose Canvas 2D:** Cách `SceneCoordinateMapper` chuyển tọa độ mét → pixel
6. **R19 Provenance Guard:** `source == "v2x_relayed"` — tại sao quan trọng, attack vector là gì?

---

### 🟠 BẠN ĐỒNG ĐỘI — `feat/phase5-ivi-hmi-complete`

**Mục tiêu chính: Hoàn thiện WarningView mount + Mock sender fix + Deployment + Presentation**

#### Task B-1: Mount `CanvasWarningView` thực sự vào `WarningView` slot (30 phút)

Trong `MainScreen.kt` dòng 231, hiện tại:
```kotlin
DisplayMode.WarningView -> WarningViewContent(...)
// WarningViewContent gọi warningViewSeam?.Render() hoặc fallback WarningViewPlaceholder()
```

Kiểm tra xem `warningViewSeam` có được inject đúng vào `MainScreen()` chưa:
- Nếu `warningViewSeam = null` → placeholder vẫn hiện → cần xem lại `MainActivity.kt`
- Đảm bảo `CanvasWarningView` được bind qua Hilt `AppModule`

#### Task B-2: Fix mock sender schema (§4.4 MOCK-1) (45 phút)

**File:** `IVI_ECU/mock-sender/mock_r4_sender.py`

Vấn đề: `object.class` field bị thiếu (R4Deserializer yêu cầu `objectClass`). Check lại toàn bộ payload match với `contracts/r4-ada-ivi.schema.json`:
- `"object"` → phải có `"class"` field
- `"object"` → `"timestamps"` object với `measured`, `received`, `lastUpdated`
- Test: chạy sender → xem logcat `R4Deserializer` có log `parse error` không

#### Task B-3: Deploy lên CarSky AAOS node + Smoke Test (2–3 giờ)

Theo `documents/Delivery/Test-Guides/apk-deploy.md`:
1. Build APK: `./gradlew assembleDebug`
2. Connect ADB tunnel tới CarSky Room
3. Install: `adb install -r app-debug.apk`
4. Launch app, xem R16 layout hiện đúng
5. Chạy mock sender Python, verify:
   - `WarningView` (God-View) tự hiện khi nhận R4
   - Tự về `HomeView` sau 10 giây
   - Logcat: `R4ListenerService`, `WarningViewModel`, `IVI_V2X` không có ERROR

**Ghi lại evidence:** Chụp màn hình AAOS + logcat để đưa vào báo cáo.

#### Task B-4: Presentation Deck Phase 5 (2–3 giờ)

**File:** `presentation/phase5/phase5-ivi-deck.md` (NEW)

Cấu trúc slide theo mẫu anh Minh (phase0, phase1):
1. **Problem Statement** — V2X NLOS warning challenge
2. **Architecture Overview** — 4-node topology, luồng R4 ADA→IVI
3. **Demo Flow** — Idle Dashboard → R4 arrives → God-View warning → timeout → Dashboard
4. **Key Technical Decisions** — UDP vs SOME/IP, MVVM, Compose Canvas, R19 guard
5. **Test Evidence** — Unit test matrix GREEN, integration test
6. **Deploy Evidence** — APK size, ADB install, AAOS screenshot
7. **What We Learned** — AAOS, Kotlin Flow, Jetpack Compose Canvas, provenance safety

---

## 🔄 Flow Làm Việc

```
      VINH                           BẠN ĐỒNG ĐỘI
       │                                   │
  V-1: Merge fixes ──────────────────────> pull merge
       │                                   │
  V-2: HomeView UI                    B-1: Mount CanvasWarningView
       │                                   │
  V-3: Tech Wiki                      B-2: Fix mock sender schema
       │                                   │
       └───── Review chéo lẫn nhau ───────┘
                        │
                   B-3: Deploy to AAOS
                        │
                   B-4: Presentation deck
                        │
              PR → main (trước 10/08)
```

---

## 📋 Checkpoint Review Chéo

Sau khi mỗi người xong task của mình, đổi sang review task của người kia:

| Vinh review | Bạn đồng đội review |
|---|---|
| `CanvasWarningView` mount đúng chưa (B-1) | `HomeView` có đúng spec không (V-2) |
| Mock sender schema đúng contract chưa (B-2) | Tech wiki có trả lời đủ câu hỏi anh Minh không (V-3) |
| Deploy evidence đầy đủ không (B-3) | Bug fixes có conflict gì không (V-1) |

---

## 🏁 Definition of Done (Anh Minh's criteria)

- [ ] `./gradlew test` — ALL GREEN trên cả 2 máy
- [ ] APK install và chạy trên AAOS node (screenshot làm evidence)
- [ ] Demo flow hoạt động: Idle → Warning (sau khi nhận UDP) → Idle (sau 10s)
- [ ] Mock sender gửi được packet, IVI nhận và chuyển màn hình
- [ ] Presentation deck có đủ: Architecture + Demo evidence + Kỹ thuật tường minh
- [ ] Technical Wiki giải thích được: AAOS, MVVM, UDP pipeline, Compose Canvas
- [ ] PR lên `main` với commit history rõ ràng, không có conflict

---

## 📚 Tài liệu Tham khảo cho Nghiên cứu

Câu hỏi kỹ thuật để prompt AI (theo gợi ý anh Minh):

| Chủ đề | Câu hỏi để hỏi AI |
|---|---|
| AAOS | "Explain AAOS vs AOSP Android — what makes AAOS special for IVI systems?" |
| UDP pipeline | "How does Android deserialize Ethernet bytes from a UDP socket into a Kotlin data class?" |
| MVVM | "Explain MVVM pattern in Android with StateFlow and Jetpack Compose" |
| SOME/IP vs UDP | "Why is raw UDP preferred over SOME/IP for low-latency V2X warning in AAOS?" |
| Compose Canvas | "How does Jetpack Compose Canvas coordinate system work for 2D vehicle rendering?" |
| Hilt DI | "How does Hilt dependency injection work in an Android Automotive Service + ViewModel?" |
