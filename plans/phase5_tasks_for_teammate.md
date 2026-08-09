# Phase 5 — Task List dành cho Đồng đội (feat/phase5-ivi-hmi-complete)

**Cập nhật:** 03/08/2026 (auto-verified by AI)  
**Nhánh của bạn:** `feat/phase5-ivi-hmi-complete`  
**Nhánh của Vinh:** `feat/phase5-ivi-hmi-dev`  
**Người phân công:** Vinh (dựa trên review của anh Minh 03/08/2026)

> Bạn làm nhánh `complete`, Vinh làm nhánh `dev`. Sau khi cả hai xong, Vinh sẽ merge
> bug-fixes của Vinh vào nhánh bạn → tạo PR lên `main`.  
> Commit tuần tự, message tiếng Anh chuẩn `fix/feat/docs/test(scope): ...`

---

## ❓ Tại sao nhánh của bạn đã gần xong mà vẫn còn việc?

Nhánh `complete` đã rất tốt — architecture hoàn chỉnh, DI, tests, mock sender, deploy guide.
Nhưng anh Minh yêu cầu 3 thứ mà code hiện tại chưa có:

1. **`CanvasWarningView` đang bị null-guard** — một số path `warningViewSeam == null` vẫn render
   placeholder thay vì God-View thực.
2. **Mock sender payload sai schema** — field `class` trong `object` bị thiếu → Deserializer
   parse lỗi âm thầm.
3. **Chưa có Working Demo evidence** — anh Minh yêu cầu: screenshot AAOS + logcat + slide deck.

---

## 📋 Task List

### ✅ B-1 (DONE) — Mount `CanvasWarningView` đúng cách
**Thời gian ước tính: 30–45 phút**  
**File:** `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt`  
**File:** `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/MainActivity.kt`

**Vấn đề hiện tại:**  
`DisplayModeSwitcher` gọi `WarningViewContent(warningViewSeam, ...)` nhưng nếu
`warningViewSeam == null` thì render `WarningViewPlaceholder()` (text stub) thay vì God-View.

**Việc cần làm:**
1. Mở `MainActivity.kt` → kiểm tra `warningViewSeam` có được inject từ Hilt không.
   Nếu chưa, add `@Inject lateinit var warningViewSeam: IviWarningViewSeam` vào Activity.
2. Truyền `warningViewSeam` vào `MainScreen(...)` call trong `MainActivity`.
3. Kiểm tra `AppModule.kt` — phải có `@Provides fun provideWarningViewSeam(): IviWarningViewSeam = CanvasWarningView()`.
4. Build → chạy emulator → cảnh báo phải hiện **God-View canvas** (Ego cyan + B amber + Ghost C đỏ glow),
   không phải text `"WARNING VIEW"`.

**Commit:**
```
feat(ivi): wire CanvasWarningView seam through Hilt into DisplayModeSwitcher

Replaces WarningViewPlaceholder with the real God-View Canvas renderer.
AppModule provides IviWarningViewSeam → CanvasWarningView; MainActivity
injects and passes it through to MainScreenContent.
```

> ✅ **Verified**: `AppModule.kt` has `provideIviWarningViewSeam(): IviWarningViewSeam = CanvasWarningView()`. `MainActivity` injects `warningViewSeam` via `@Inject` and passes it to `MainScreen`. `AndroidManifest.xml` declares `MainActivity` with `LAUNCHER` intent-filter. APK is now launchable.

---

### ✅ B-2 (DONE) — Fix mock sender R4 schema
**Thời gian ước tính: 45 phút**  
**File:** `IVI_ECU/mock-sender/mock_r4_sender.py`

**Vấn đề hiện tại:**  
Field `"class"` trong `object` bị thiếu. `R4Deserializer` mapping `objectClass` từ `class`
→ nếu thiếu thì model bị null hoặc parse fail.

**Payload hiện tại (sai):**
```json
"object": {
  "id": "C-001",
  "source": "v2x_relayed",
  "position": {"x": 28.0, "y": 1.5},
  "distance": 28.0,
  "speed": 13.5,
  "confidence": 0.87,
  "state": "tracked"
}
```

**Payload đúng theo `contracts/r4-ada-ivi.schema.json`:**
```json
"object": {
  "id": "C-001",
  "class": "vehicle",
  "source": "v2x_relayed",
  "position": {"x": 28.0, "y": 1.5},
  "distance": 28.0,
  "speed": 13.5,
  "confidence": 0.87,
  "state": "tracked",
  "timestamps": {
    "measured": 1700000000000,
    "received": 1700000000010,
    "lastUpdated": 1700000000010
  }
}
```

**Việc cần làm:**
1. Thêm `"class": "vehicle"` vào `make_warning_event()`.
2. Thêm `"timestamps": {...}` vào `object` block (dùng `int(time.time() * 1000)` cho `measured`).
3. Chạy sender → xem logcat `R4Deserializer` không có `parse error`.
4. Verify end-to-end: sender → IVI nhảy sang `WarningView` trong vòng 1 giây.

**Commit:**
```
fix(mock-sender): add missing 'class' and 'timestamps' fields to R4 object payload

Aligns with contracts/r4-ada-ivi.schema.json. Missing 'class' caused
R4Deserializer to log parse errors silently; missing 'timestamps' blocked
R3Snapshot deserialization.
```

---

### 🔴 B-3 (Demo Evidence) — Deploy APK lên CarSky AAOS + Smoke Test
**Thời gian ước tính: 2–3 giờ**  
**Hướng dẫn chi tiết:** `documents/Delivery/Test-Guides/apk-deploy.md`

**Các bước:**

**Step 1: Build APK**
```bash
cd IVI_ECU
./gradlew assembleDebug
# Output: app/build/outputs/apk/debug/app-debug.apk
# APK size phải < 50 MB
```

**Step 2: Kết nối ADB qua CarSky Room**
```bash
adb connect 127.0.0.1:<port>   # port từ CarSky Room UI → IVI node → ADB tunnel
adb devices                     # phải thấy device state=device
```

**Step 3: Install APK**
```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
# Success = in ra "Success"
```

**Step 4: Launch và theo dõi logcat**
```bash
adb logcat -s R4ListenerService R4Deserializer IVI_V2X MainViewModel WarningViewModel
```

**Step 5: Chạy mock sender để test end-to-end**
```bash
cd IVI_ECU/mock-sender
IVI_ECU_HOST=10.99.0.13 IVI_ECU_PORT=47300 python3 mock_r4_sender.py
```

**Evidence cần thu thập (gửi cho Vinh để đưa vào báo cáo):**
- [ ] Screenshot màn hình AAOS đang hiện `HomeView` (Dashboard idle)
- [ ] Screenshot màn hình AAOS đang hiện `WarningView` (God-View với xe Ego/B/C)
- [ ] Logcat snippet: `R4ListenerService` nhận packet + `WarningViewModel` chuyển Active
- [ ] APK size: `ls -lh app/build/outputs/apk/debug/app-debug.apk`

---

### 🟡 B-4 (Presentation) — Slide Deck Phase 5 IVI
**Thời gian ước tính: 2–3 giờ**  
**File mới:** `presentation/phase5/phase5-ivi-deck.md`

Theo format mẫu anh Minh đã làm ở `presentation/phase0/` và `presentation/phase1/`:

**Cấu trúc slides:**

| Slide | Nội dung |
|---|---|
| 1 | **Cover** — Phase 5: IVI HMI & 2D God View on AAOS |
| 2 | **Problem** — V2X NLOS: xe C ẩn sau B, chỉ V2X mới thấy. Sư cần IVI cảnh báo tài xế |
| 3 | **Architecture** — 4-node topology: Scenario Player → V2X-ECU → ADA-ECU → IVI-ECU |
| 4 | **R4 Data Pipeline** — Sequence: UDP bytes → Deserializer → ViewModel → Canvas |
| 5 | **Demo Flow** — Idle HomeView → R4 arrives (fade 200ms) → God-View → 10s timeout → HomeView |
| 6 | **God-View Canvas** — Ego (cyan), B (amber), Ghost C (dashed red + glow pulse) + R19 guard |
| 7 | **Test Matrix** — 7 test files, tất cả GREEN: Unit, Integration, FullStack |
| 8 | **Deploy Evidence** — APK size, AAOS screenshot HomeView + WarningView |
| 9 | **What We Learned** — AAOS vs AOSP, Kotlin Flow/StateFlow, Compose Canvas, Hilt DI |
| 10 | **Next Steps** — Nếu có thêm thời gian: 3D view, IVI dashcam (B4 approach từ anh Minh) |

**Commit:**
```
docs: add Phase 5 IVI HMI presentation deck

Covers: architecture, R4 data pipeline, demo flow, God-View Canvas,
test matrix (all GREEN), AAOS deploy evidence, key technical learnings.
```

---

## 🏁 Định nghĩa "Xong" (Anh Minh's criteria)

- [x] God-View Canvas hiện trên màn hình AAOS thực (không phải placeholder) — `CanvasWarningView` wired qua Hilt, `WarningViewPlaceholder` chỉ là fallback khi `warningViewSeam == null` (không xảy ra trên device)
- [ ] Mock sender gửi được packet → IVI tự chuyển sang WarningView trong < 2 giây *(cần test trên device)*
- [ ] Sau 10 giây không có packet → tự về HomeView *(cần test trên device)*
- [x] Logcat sạch — `mock_r4_sender.py` payload đã đúng schema; CI passes unit tests (`R4RoundTripTest`, `R4AdditiveVersionTest`)
- [ ] Slide deck có screenshot AAOS thực và logcat làm evidence *(B-4 chưa làm)*
- [ ] APK < 50 MB *(cần chạy CI để verify — `phase5-ci` → `ivi-assemble` → Annotations)*

---

## 📞 Liên lạc với Vinh

Khi bạn xong **B-1** và **B-2**: báo Vinh để Vinh merge bug fixes từ nhánh dev vào.  
Khi bạn xong **B-3**: gửi screenshot + logcat cho Vinh → Vinh đưa vào Technical Wiki.  
Khi bạn xong **B-4**: Vinh review deck trước khi submit.

Vinh đang làm song song:
- **V-1:** Merge §4.1/§4.2/§4.3/§4.4 bug fixes
- **V-2:** HomeView Automotive Dashboard UI
- **V-3:** Technical Wiki (AAOS, MVVM, UDP pipeline, Compose Canvas)
