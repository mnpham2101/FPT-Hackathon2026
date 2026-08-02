# Bản kế hoạch hành động Phase 5 — Vũ Xuân Bách

> **Authority:** Lead trên `origin/main` → [`phase5_minh_tasks.md`](phase5_minh_tasks.md) (+ [`ada-ivi-plan.md`](ada-ivi-plan.md)). **Không sửa** file Lead.
>
> **Branch:** `feat/phase5-ivi-hmi-complete` đã **rebase lên `origin/main`** (2026-08-02). Conflict `R4Message.kt` / `SceneGeometry.kt` → lấy bản **main** (Authority Contract), rồi chỉnh consumer/UI cho khớp.
>
> Chỉ cập nhật trạng thái tại file này.

---

## Gap vs Lead (sau rebase)

| Chủ đề | Trạng thái |
| :--- | :--- |
| Authority Contract R4 (`R4Message` / `SceneGeometry` = main) | **[x] done** |
| IP IVI `10.99.0.13` + UDP default `47300` (+ `local.properties` override) | **[x] done** |
| Bỏ `StandbyView` / Video; default `HomeView`; status `BOUND :<port>` | **[x] done** |
| Wake-on-Warning + restore `previousMode` | **[x] done** |
| `vehicleCSnapshot` → Canvas (R19) | **[x] done** |
| Live `R4LinkState` / module split / `IviGraph` | **[ ] pending — lane Vinh/shared** |

---

## Phân công

### Bách — `[x] done`

- [x] **B.1** Gradle AAOS + `BuildConfig.R4_UDP_PORT=47300` (+ override `r4.udp.port`)
- [x] **B.2** UDP FGS listener + R4 Deserializer (kotlinx.serialization; preserve unknown `warningType`)
- [x] **B.3** Coordinate mapper + 2D Canvas God View + defensive `v2x_relayed` guard
- [x] **B.4** HMI: bỏ Standby/Video; default `HomeView`; status `V2X LINK: BOUND :{port}`
- [x] **B.5** Wake-on-Warning Active→`WarningView`; Idle→restore (default Home)
- [x] **B.6** `WarningViewModel` wires `vehicleCSnapshot = objectSnapshot`
- [x] **B.7** Mock `10.99.0.13:47300` + deploy doc + unit/integration tests
- [x] **B.8** Rebase lên main; prefer main R4 contracts; adapt local consumers

### Vinh / Shared — `[ ] pending - lane Vinh`

- [ ] **V.1** Module split (`:contract` / `:serializer` / `:observer` / `:r4-simulator`)
- [ ] **V.2** Kotlin `:r4-simulator` (thay Python mock dài hạn)
- [ ] **V.3** Live `R4LinkState` (`BOUND` / `REBINDING` / `ERROR`) → status bar
- [ ] **V.4** Remove Hilt → `IviGraph` (HLD D7 / `4.5.1.2` + `4.5.5.3`)
- [ ] **V.5** `IviRuntimeConfig` + `--ei r4_port` + full D10 BuildConfig knobs
- [ ] **V.6** phase5-ci + in-Room evidence (groups 5.7–5.9)

---

## Port override

```properties
# IVI_ECU/local.properties — chỉ khi cần lệch khỏi default 47300
# r4.udp.port=5004
```

Default build = **47300** (blueprint ADA→IVI `10.99.0.13:47300`).
