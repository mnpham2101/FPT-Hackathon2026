# Phase 5 — Progress Report (Vinh's branch: `feat/phase5-ivi-hmi-dev`)

> **Mục đích:** File này track trạng thái THỰC TẾ của nhánh Vinh so với checklist của anh Minh trong
> [`phase5_minh_tasks.md`](phase5_minh_tasks.md). Không sửa file của anh Minh.
>
> **Cập nhật lần cuối:** 03/08/2026 — auto-verified bởi AI  
> **Nhánh:** `feat/phase5-ivi-hmi-dev` (commit `f72d8a6`)  
> **Kiến trúc hiện tại:** Single-module (`:app`) — chưa refactor sang multi-module như anh Minh thiết kế

---

## ✅ 5 Phase Acceptance Boxes (Output chính của phase)

> Nguồn: `phase5_minh_tasks.md § Phase 5 overview`

| # | Acceptance Box | Trạng thái | Ghi chú |
|---|----------------|-----------|---------|
| 1 | HMI chạy trên AAOS node với R16 layout; button/app areas switch Display Area | ✅ **Done** | `MainScreen.kt` + `MainViewModel.kt` + `HomeScreen.kt` |
| 2 | Mock R4 warning → Warning View với ego, B, ghost C ở đúng vị trí | ✅ **Done (code)** | `CanvasWarningView.kt` + `WarningViewModel.kt` |
| 3 | Ghost C render từ `v2x_relayed` data only; 2D drawing delivered (R17) | ✅ **Done** | R19 provenance guard trong `CanvasWarningView` |
| 4 | Unknown `warningType` degrades gracefully (R4 additive-version test) | ✅ **Done** | `R4AdditiveVersionTest` passes; `WarningClassifier` giữ nguyên wire value |
| 5 | Optional: ADA wakes separate warning app; 3D via view seam | ⏭️ **Optional** | Không làm trong phase này |

---

## 📋 Task Groups — Chi tiết

### Group 5.1 — Gradle Multi-Module Foundation

> Anh Minh yêu cầu refactor thành 5 module: `:contract`, `:serializer`, `:observer`, `:r4-simulator`, `:app`

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `4.5.1.1` | Version catalog `libs.versions.toml` | ❌ Chưa làm |
| `4.5.1.2` | Move `:app` onto catalog, drop Hilt | ⚠️ Partial — Hilt đang được dùng (`@Inject`, `@AndroidEntryPoint`) |
| `4.5.1.3` | Tạo module `:contract` | ❌ Chưa làm |
| `4.5.1.4` | Relocate models/tests/samples vào `:contract` | ❌ Chưa làm |
| `4.5.1.5` | ProGuard keep rules cho relocated models | ❌ Chưa làm |

> ⚠️ **Ghi chú:** Nhánh hiện tại vẫn dùng Hilt (anh Minh muốn xóa Hilt → dùng `IviGraph` tự viết). Đây là sự khác biệt kiến trúc lớn nhất.

---

### Group 5.2 — `:serializer` Module

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `4.5.2.1` | Module `:serializer` + `R4Decoder` interface | ❌ Chưa làm (tương đương code nằm trong `:app`) |
| `4.5.2.2` | `R4Deserializer` implementation + decode-table test | ⚠️ Partial — `R4Deserializer.kt` tồn tại trong `:app/data/` nhưng chưa tách module |
| `4.5.2.3` | Buffer-slicing test | ❌ Chưa có test này |

---

### Group 5.3 — `:observer` Module

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `4.5.3.1` | Module `:observer` + seams, config, event types | ❌ Chưa làm (tương đương nằm trong `:app`) |
| `4.5.3.2` | `JdkDatagramSource` với `setLength` rule | ❌ Chưa làm (logic nằm trong `R4ListenerService`) |
| `4.5.3.3` | `R4SocketObserver` — receive loop + event flow | ❌ Chưa tách (fused vào `R4ListenerService`) |
| `4.5.3.4` | Rebind back-off + `R4LinkState.Error` reachable | ❌ Chưa làm |
| `4.5.3.5` | Loopback socket test (I2) | ❌ Chưa có |

---

### Group 5.4 — `:app` Data & Logic Layer

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `4.5.4.1` | `IviRuntimeConfig` + `BuildConfig` fields (D10) | ✅ **Done** — `BuildConfig.WARNING_TIMEOUT_MS`, `R4_UDP_PORT=47300` |
| `4.5.4.2` | `R4Repository` — routing point | ✅ **Done** — `R4Repository.kt` có `warnings`, `lastState`, `linkState` |
| `4.5.4.3` | `WarningClassifier` — presentation mapping (D4) | ✅ **Done** — không rewrite `warningType`, giữ wire value |
| `17.5.4.4` | `WarningViewModel` + R19 snapshot wiring | ✅ **Done** — `vehicleCSnapshot = warning.objectSnapshot` compose đúng |
| `16.5.4.5` | `MainViewModel` — wake-on-warning, restore, user override | ✅ **Done** — `previousMode`, `userOverrodeDuringWarning` |

---

### Group 5.5 — `:app` Host Layer (Activity, Service, Seams)

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `4.5.5.1` | `AndroidR4Logger` — bridge `R4Logger` → `android.util.Log` | ⚠️ Partial — dùng `safeLog` wrapper, chưa có class riêng |
| `4.5.5.2` | Wire observer vào `R4ListenerService` | ✅ **Done** — foreground service + UDP socket |
| `4.5.5.3` | `IviGraph` — manual DI (thay Hilt theo D7) | ❌ Chưa làm — vẫn dùng Hilt |
| `4.5.5.4` | `IviApplication.kt` | ✅ **Done** |
| `4.5.5.5` | `MainActivity` + launcher intent-filter | ✅ **Done** — `LAUNCHER` filter trong `AndroidManifest.xml` |
| `4.5.5.6` | Bottom status bar bind to real `R4LinkState` | ⚠️ Partial — hardcoded `V2X LINK: STANDBY` |
| `16.5.5.7` | `MainScreen.kt` R16 layout complete | ✅ **Done** — scaffold với Display Area, side buttons, status bar |

---

### Group 5.6 — R4 Simulator & Dev Injector

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `4.5.6.1`–`4.5.6.6` | `:r4-simulator` module + Docker image `m1-r4-sim:latest` | ❌ Chưa làm |
| `4.5.6.7` | Dev injector broadcast (injection point I3) | ❌ Chưa làm |

---

### Group 5.7 — CI Lane

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `16.5.7.1` | `phase5-ci.yml` với `ivi-assemble` lane | ✅ **Done** — unit tests gate + APK artifact + size notice |

---

### Group 5.8 — Deploy & In-Room Verification

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `5.5.8.1` | Mini-blueprint (3-node) deploy guide | ❌ Chưa có |
| `5.5.8.2` | ADB tunnel proven (biggest schedule risk `16.5.8.3`) | ❌ Chưa verify |
| `16.5.8.3` | ADB route proven early (§4.4 walkthrough) | ❌ USER-MANUAL — cần CarSky Room running |
| `16.5.8.4` | Run record document | ❌ Chưa có |

---

### Group 5.9 — Evidence & Acceptance

| Task | Mô tả | Trạng thái |
|------|--------|-----------|
| `16.5.9.1` | Screenshot AAOS HomeView + WarningView | ❌ USER-MANUAL — cần deploy |
| `16.5.9.2` | Boot-to-listener timing recorded | ❌ USER-MANUAL |
| `17.5.9.3` | Screen recording với `cSource=v2x_relayed` logcat | ❌ USER-MANUAL |

---

## 🏗️ Sự khác biệt kiến trúc với thiết kế của Anh Minh

| Điểm khác | Anh Minh muốn | Vinh đang làm | Ảnh hưởng |
|-----------|---------------|---------------|-----------|
| **Dependency Injection** | `IviGraph` tự viết (D7 — remove Hilt) | Hilt (`@AndroidEntryPoint`, `@Inject`) | Medium — DI vẫn work |
| **Module structure** | 5 modules (`contract`, `serializer`, `observer`, `r4-simulator`, `app`) | 1 module (`:app`) | Medium — khó test observer không cần device |
| **Socket loop** | `JdkDatagramSource` + `R4SocketObserver` tách riêng | Fused vào `R4ListenerService` | Low — functional |
| **Logging** | `AndroidR4Logger` seam | `safeLog` wrapper | Low — functional |

---

## 🎯 Ưu tiên nếu còn thời gian (trước deadline 08/08)

1. **🔴 Cao nhất** — Deploy lên CarSky Room, chụp screenshot AAOS (Groups 5.8 + 5.9) → evidence duy nhất chứng minh acceptance boxes
2. **🟡 Trung bình** — Fix status bar `V2X LINK:` bind vào `R4LinkState` thật (`4.5.5.6`)
3. **🟢 Thấp** — Multi-module refactor (Groups 5.1–5.3) nếu anh Minh yêu cầu strict compliance
