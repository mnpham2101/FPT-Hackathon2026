# Phase 5 minh — IVI implementation audit

Read-only compare of `IVI_ECU/` against [phase5_minh_tasks.md](../../../plans/phase5_minh_tasks.md) (post gap-fix). Status for planning only — minh subtasks remain **Status: pending** until execution closes them.

**High-level:** ~**75–80%** of committed (non-optional) Phase 5 IVI **code** is present; **critical open gaps** are production port **47300** override, Jacoco **≥80%** gate, banner **not mounted**, and CarSky **Room/deploy evidence** (image push, pins, ADB-tunnel install smoke, Room HMI logcat).

---

## Task Group 5.1 — Scaffold + ports

| ID | Verdict | Evidence / gap |
|---|---|---|
| `16.5.1.1` | **Done** | `app/build.gradle.kts` — Compose/Hilt/serialization, `BuildConfig.R4_UDP_PORT=5004`, `WARNING_TIMEOUT_MS=10000L`, proguard on release |
| `4.5.1.2` | **Not done** | No product flavor / documented field override producing APK with port **47300**; deploy doc mentions 5004 default only |
| `16.5.1.3` | **Done** | `IviApplication.kt`, `MainActivity.kt`, `AndroidManifest.xml` (`automotive` feature, `INTERNET`, `FOREGROUND_SERVICE*`), `network_security_config.xml` |

---

## Task Group 5.2 — Contract / parse

| ID | Verdict | Evidence / gap |
|---|---|---|
| `4.5.2.1` | **Done** | `model/R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt` |
| `4.5.2.2` | **Done** | `data/R4Deserializer.kt` — additive-version degrade, malformed → `Result.failure` |
| `4.5.2.3` | **Done** | `R4DeserializerTest.kt`, `model/R4RoundTripTest.kt` |
| `4.5.2.4` | **Done** | `model/R4AdditiveVersionTest.kt` |
| `4.5.2.5` | **Not done** | No `:r4-contract` module (optional / non-gating) |

---

## Task Group 5.3 — UDP adapter

| ID | Verdict | Evidence / gap |
|---|---|---|
| `4.5.3.1` | **Done** | `service/R4ListenerService.kt` — foreground, `BuildConfig` port, reconnect/backoff comments + logic, `onDestroy` close |
| `4.5.3.2` | **Done** | Same service — UTF-8 → deserializer → flow emit |
| `4.5.3.3` | **Partial** | Malformed/empty covered mainly via `R4DeserializerTest.kt`; no dedicated service-helper empty/malformed class named in minh |

---

## Task Group 5.4 — Observer / VMs / Hilt

| ID | Verdict | Evidence / gap |
|---|---|---|
| `4.5.4.1` | **Done** | `data/R4Repository.kt` + `R4RepositoryTest.kt` |
| `4.5.4.2` | **Partial** | `di/AppModule.kt` binds deserializer, repository, seam; `MainActivity` starts/binds service — but `MainViewModel` uses manual `ViewModelProvider.Factory`, not `@HiltViewModel` |
| `4.5.4.3` | **Done** | `ui/WarningViewModel.kt`, `WarningUiState.kt`, `WarningViewModelTest.kt` — timeout from `BuildConfig.WARNING_TIMEOUT_MS` |
| `16.5.4.4` | **Done** | `ui/MainViewModel.kt`, `DisplayMode.kt`, `ui/MainViewModelTest.kt` — `previousMode`, `userOverrodeDuringWarning` |

---

## Task Group 5.5 — R16 shell

| ID | Verdict | Evidence / gap |
|---|---|---|
| `16.5.5.1` | **Done** | `ui/screen/MainScreen.kt` — Display area + chrome |
| `16.5.5.2` | **Done** | Chrome → `setMode`; `AnimatedContent` switches Display |
| `16.5.5.3` | **Done** | Wake-on-warning via MainViewModel collecting WarningViewModel; override path tested |

---

## Task Group 5.6 — R17 Canvas / seam / tests

| ID | Verdict | Evidence / gap |
|---|---|---|
| `17.5.6.1` | **Done** | `ui/view/IviWarningViewSeam.kt` |
| `17.5.6.2` | **Done** | `ui/view/SceneCoordinateMapper.kt` + `SceneCoordinateMapperTest.kt` |
| `17.5.6.3` | **Done** | `ui/view/CanvasWarningView.kt` — ego/B/ghost Canvas God View |
| `17.5.6.4` | **Done** | Guard + `[? UNKNOWN SOURCE]`; `CanvasWarningViewTest.kt` (`own_sensor`) |
| `17.5.6.5` | **Partial** | `WarningBannerOverlay.kt` implements countdown / X-dismiss-banner-only / risk colors — **not mounted** in `MainScreen` (explicit comment; God-View-only choice) |
| `4.5.6.6` | **Done** | `integration/FullStackIntegrationTest.kt` |
| `17.5.6.7` | **Not done** | No Jacoco/coverage Gradle task; no ≥80% gate in tree |

---

## Task Group 5.7 — Mock harness

| ID | Verdict | Evidence / gap |
|---|---|---|
| `4.5.7.1` | **Done** | `mock-sender/mock_r4_sender.py` |
| `4.5.7.2` | **Done** | `mock-sender/Dockerfile` |
| `4.5.7.3` | **Done** | `mock-sender/README.md` |
| `4.5.7.4` | **Partial** | Deploy doc has local loopback commands; no committed logcat/screenshot smoke evidence section closing the subtask |

---

## Task Group 5.8 — CarSky 2-node + APK

| ID | Verdict | Evidence / gap |
|---|---|---|
| `16.5.8.1` | **Partial** | APK build path exists (`assembleDebug` / `app/build/outputs/apk/…`); treat as reproducible artifact, not a closed evidence commit |
| `4.5.8.2` | **Not done** | No recorded CarSky registry push of `m1-mock-r4-sender` |
| `16.5.8.3` | **Not done** | No recorded 2-node pin-wiring evidence commit |
| `16.5.8.4` | **Not done** | No Room Running acceptance evidence |
| `16.5.8.5` | **Partial** | `deployment/phase5-ivi-deploy.md` documents ADB tunnel + install — no proof install succeeded on live Skycraft |
| `4.5.8.6` | **Not done** | No Room HMI + additive-version logcat evidence |

Deploy doc present: `deployment/phase5-ivi-deploy.md` (+ narrative `phase5_completion_report.md`). Ops path incomplete vs minh 5.8 acceptance.

---

## Task Group 5.9 — Optional mini ADA+IVI

| ID | Verdict | Evidence / gap |
|---|---|---|
| `4.5.9.1` | **Partial** | Research note `doc/research_notes/phase5-mini-blueprint-ada-ivi.md` exists; not framed as deploy-folder prep closing the subtask |
| `4.5.9.2` | **Not done** | Blocked / optional — no mini Room deploy |

---

## Task Group 5.10 — Optional 3D / multi-process

| ID | Verdict | Evidence / gap |
|---|---|---|
| `17.5.10.1` | **Not done** | No `SceneViewWarning3D` / `ENABLE_3D_VIEW` |
| `16.5.10.2` | **Not done** | Single-app only |

---

## Skim checklist (files)

| Area | State |
|---|---|
| Models | Present under `model/` |
| Deserializer | Present + tests |
| Service | Present + reconnect |
| Repository | Present + tests |
| ViewModels | Present + tests; MainVM not Hilt-injected |
| MainScreen | R16 shell + wake wiring |
| Canvas / seam / mapper | Present; ghost guard present |
| Banner | Implemented, unmounted |
| Hilt | `AppModule` + `@AndroidEntryPoint`; MainVM factory gap |
| Tests | Broad unit + full-stack; no coverage gate |
| mock-sender | Python + Docker + README |
| BuildConfig ports | 5004 + timeout only; no 47300 path |
| Deploy docs | Procedure written; Room evidence missing |

---

## Critical gaps still open

1. **`4.5.1.2`** — BuildConfig/flavor override for **47300**.
2. **`17.5.6.7`** — package coverage ≥80% not enforced.
3. **`17.5.6.5`** — banner policy code exists but Display host does not mount it.
4. **Group 5.8** — CarSky image push, pin wiring, Room Running, ADB install proof, Room HMI/additive-version smoke.
5. **Optional** — `:r4-contract`, 3D stub, multi-process wake, mini ADA+IVI Room (non-gating).
