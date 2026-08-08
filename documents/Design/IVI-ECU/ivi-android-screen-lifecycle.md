# Wiki — How one screen appears in the IVI Android app

**Audience:** learning note for the delivery report (code freeze).  
**Author:** Vũ Xuân Bách · Phase 5 IVI  
**Scope:** the shipped APK under `IVI_ECU/app/` on AAOS (Skycraft).  
**Authorities:** [AndroidManifest.xml](../../../IVI_ECU/app/src/main/AndroidManifest.xml) · [MainActivity.kt](../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/MainActivity.kt) · [MainScreen.kt](../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt) · [DisplayMode.kt](../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/DisplayMode.kt) · [ivi-ecu-hld.md](ivi-ecu-hld.md)

This note answers: *what happens from process start until the driver sees Home or the God View?* It is not a second HLD.

---

## 1. Platform constraint: AAOS, not a phone app

| Requirement | Manifest / code fact |
| --- | --- |
| Head-unit OS only | `uses-feature android.hardware.type.automotive` with `required=true` |
| Network for R4 UDP | `INTERNET` |
| Listener stays up during the recorded run | `FOREGROUND_SERVICE` + `FOREGROUND_SERVICE_DATA_SYNC` + service `foregroundServiceType=dataSync` |
| Application class | `android:name=".IviApplication"` (Hilt / app graph) |

A phone image without the automotive feature rejects the APK at install. The CarSky Skycraft guest is the intended host ([node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md)).

---

## 2. Declaring “the screen” — Manifest, not a layout XML file

Android does not start from an arbitrary Kotlin file. The system reads the package’s **AndroidManifest.xml**, finds an **activity** with a launcher **intent-filter**, and starts that component.

```xml
<activity android:name=".MainActivity" android:exported="true">
    <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
    </intent-filter>
</activity>

<service
    android:name=".service.R4ListenerService"
    android:foregroundServiceType="dataSync"
    android:exported="false" />
```

| Concept Lead asked about | Role in this app |
| --- | --- |
| **Manifest** | Declares the only Activity and the R4 foreground Service; permissions; automotive feature |
| **Activity** | `MainActivity` — process UI entry; hosts Compose |
| **Intent (launcher)** | `MAIN` + `LAUNCHER` — why the app icon / `am start …/.MainActivity` opens this screen |
| **Intent (service)** | `Intent(this, R4ListenerService::class.java)` — startForegroundService + bindService |
| **Fragment** | **Not used.** One Activity; views swap inside Compose by `DisplayMode` |
| **Multi-Activity** | **Not used.** Home / Warning / Apps / Settings are modes of one Display Area |

Traditional phone apps often map one screen → one `Activity` or one `Fragment` transaction. This IVI app maps **one Activity → one Compose tree → many `DisplayMode` values**, which matches R16’s single Display Area with side chrome ([ivi-ecu.svg](../../../requirements/ivi-ecu.svg)).

---

## 3. Boot sequence (what runs in order)

```
AAOS PackageManager
        │  resolve MAIN/LAUNCHER
        ▼
IviApplication.onCreate   ← Application class from manifest
        │
        ▼
MainActivity.onCreate
        │  startForegroundService(R4ListenerService)
        │  setContent { … }
        ▼
Compose composition
        │  hiltViewModel<WarningViewModel>()
        │  BindR4ListenerService  (bind + attach SharedFlow)
        │  MainViewModel (factory from WarningViewModel.uiWarningState)
        │  MainScreen(…)
        ▼
First frame: DisplayMode.HomeView (default)
```

### 3.1 `MainActivity` responsibilities

- Start the UDP listener as a **foreground** service so reception is not tied to the Activity being resumed ([design D5](ivi-ecu-design-decisions.md)).
- Inject `IviWarningViewSeam` (Canvas God View implementation).
- `setContent` — replace the classic `setContentView(R.layout…)` with Jetpack Compose.
- Bind the service for the composition lifetime (`DisposableEffect` → `bindService` / `unbindService`) and call `WarningViewModel.attachService(listener.r4EventFlow)`.

### 3.2 What “a screen” means in Compose here

There is no `fragment_home.xml`. `MainScreen` reads:

- `mainViewModel.currentMode` → which pane fills the Display Area
- `warningViewModel.uiWarningState` / `latestScene` → warning content and geometry
- Side buttons call `mainViewModel.requestMode(…)` for user navigation

`DisplayMode` sealed set (Lead / HLD unchanged):

| Mode | What the Display Area shows |
| --- | --- |
| `HomeView` | Idle automotive dashboard (default) |
| `WarningView` | R17 Canvas God View via `IviWarningViewSeam` |
| `AppsView` / `SettingsView` | Placeholder panes for R16 chrome |

Wake-on-warning: `MainViewModel` collects `uiWarningState`; Active forces `WarningView` and remembers `previousMode`; Idle restores unless the driver overrode during the warning.

---

## 4. Intents — two jobs, one pattern

| Intent | Who creates it | Effect |
| --- | --- | --- |
| Launcher / `am start -n …/.MainActivity` | System or ADB | Creates or resumes `MainActivity` |
| `Intent(ctx, R4ListenerService::class.java)` | `MainActivity` | Starts FGS; bind returns `LocalBinder` → live `r4EventFlow` |
| Optional extras (`--ei r4_port`, …) | ADB / walkthrough | Runtime overrides when `IviRuntimeConfig` path is wired (HLD D10) |

No deep-link intent-filter is required for M1. Debug-only inject broadcast (if present in a debug source set) is separate from the production path — see the walkthrough’s V3 ladder.

---

## 5. Service vs UI — why the “screen” is not the listener

| Layer | Component | Lifecycle |
| --- | --- | --- |
| UI | `MainActivity` + Compose | Can pause; Display Area can show Home while UDP continues |
| Transport host | `R4ListenerService` | Foreground for the continuous R19 run |

If the receive loop lived only inside the Activity, backgrounding the UI would risk dropping ADA traffic. The Manifest-declared Service is the lifecycle host; Compose only **observes** state (see [ivi-r4-observation-pipeline.md](ivi-r4-observation-pipeline.md)).

---

## 6. Short comparison (Lead topic checklist)

| Classic Android pattern | This IVI app |
| --- | --- |
| Multiple Activities, one per screen | Single `MainActivity` |
| Fragments + `FragmentManager` transactions | Compose + `DisplayMode` + `AnimatedContent` / conditional content |
| XML layouts + `findViewById` | Compose `MainScreen` / `CanvasWarningView` |
| `startActivity` to open Warning | Mode change driven by ViewModel state (and side buttons) |
| Bound service optional | Bound FGS mandatory for R4 |

---

## 7. Where to look in the tree

```
IVI_ECU/app/src/main/
├── AndroidManifest.xml          ← declares Activity + Service + feature
└── java/com/hackathon/v2x/ivi/
    ├── IviApplication.kt
    ├── MainActivity.kt          ← Intent start/bind + setContent
    └── ui/
        ├── DisplayMode.kt
        ├── MainViewModel.kt     ← Display Area switcher + wake-on-warning
        ├── WarningViewModel.kt
        ├── screen/MainScreen.kt ← R16 chrome + Display Area
        └── view/                ← IviWarningViewSeam, CanvasWarningView, …
```

Further reading: [ivi-technical-wiki.md](ivi-technical-wiki.md) · [deploy-ivi-hmi-walkthrough.md](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) §4.7–4.8.
