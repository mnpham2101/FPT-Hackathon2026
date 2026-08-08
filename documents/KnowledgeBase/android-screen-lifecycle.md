# How one screen appears in an Android Automotive app

**Author:** Vũ Xuân Bách
**Question answered:** what happens from process start until the driver sees a screen?

Class names below are generic role names rather than any codebase's identifiers, so the arrangement reads independently of what a given project calls its types.

---

## 1. Platform constraint: AAOS, not a phone app

An automotive head-unit application declares itself as one in the manifest, and the declarations are what the platform enforces:

| Requirement | Manifest / code fact |
| --- | --- |
| Head-unit OS only | `uses-feature android.hardware.type.automotive` with `required=true` |
| Network access for the message stream | `INTERNET` |
| Listener stays up while the screen shows something else | `FOREGROUND_SERVICE` + `FOREGROUND_SERVICE_DATA_SYNC`, and `foregroundServiceType=dataSync` on the service |
| Application class | `android:name=".App"` — the object graph's root |

A phone image without the automotive feature rejects the APK at install time. That check happens at install, not at launch, so it fails on the wrong machine rather than on the wrong screen.

---

## 2. Declaring "the screen" — the manifest, not a layout file

Android does not start from an arbitrary Kotlin file. The system reads the package's **AndroidManifest.xml**, finds an **activity** carrying a launcher **intent-filter**, and starts that component.

```xml
<activity android:name=".MainActivity" android:exported="true">
    <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
    </intent-filter>
</activity>

<service
    android:name=".service.IncomingMessageListener"
    android:foregroundServiceType="dataSync"
    android:exported="false" />
```

| Concept | Role |
| --- | --- |
| **Manifest** | Declares the Activity, the foreground Service, the permissions and the automotive feature |
| **Activity** | Process UI entry; hosts the Compose tree |
| **Intent (launcher)** | `MAIN` + `LAUNCHER` — why the app icon and `am start` both open this screen |
| **Intent (service)** | `Intent(ctx, IncomingMessageListener::class.java)` — `startForegroundService` then `bindService` |
| **Fragment** | Not used. One Activity; panes swap inside Compose |
| **Multi-Activity** | Not used. The panes are modes of one display area |

A phone application often maps one screen to one `Activity`, or to one `Fragment` transaction. A head-unit application with a fixed display area maps **one Activity → one Compose tree → many display modes**, because the chrome around the display area never goes away.

---

## 3. Boot sequence

```
PackageManager
        │  resolve MAIN/LAUNCHER
        ▼
App.onCreate                  ← Application class named in the manifest
        │
        ▼
MainActivity.onCreate
        │  startForegroundService(IncomingMessageListener)
        │  setContent { … }
        ▼
Compose composition
        │  AlertViewModel        (injected)
        │  bind the service, attach its event stream
        │  ModeViewModel         (derives the mode from alert state)
        │  RootScreen(…)
        ▼
First frame: the idle mode
```

### 3.1 What the Activity is responsible for

- Start the listener as a **foreground** service, so reception does not depend on the Activity being resumed.
- Inject the renderer implementation behind its seam interface.
- Call `setContent` — the Compose entry point, replacing `setContentView(R.layout…)`.
- Bind the service for the lifetime of the composition (`DisposableEffect` → `bindService` / `unbindService`) and hand its event stream to the view model.

### 3.2 What "a screen" means under Compose

There is no `fragment_home.xml`. The root composable reads state and decides what fills the display area:

- `modeViewModel.currentMode` → which pane is shown
- `alertViewModel.uiState` → the alert content and its geometry
- Side buttons call `modeViewModel.requestMode(…)` for user navigation

**Wake-on-alert** is the one behaviour worth naming: the mode view model collects alert state, an active alert forces the alert pane and remembers the previous mode, and a returning-to-idle alert restores that mode — unless the driver navigated away during the alert, in which case their choice stands. A screen that overrides the driver twice is worse than one that never wakes.

---

## 4. Intents — two jobs, one mechanism

| Intent | Created by | Effect |
| --- | --- | --- |
| Launcher, or `am start -n …/.MainActivity` | System or ADB | Creates or resumes the Activity |
| `Intent(ctx, IncomingMessageListener::class.java)` | The Activity | Starts the foreground service; binding returns a binder exposing its event stream |
| Optional extras | ADB | Runtime overrides, where a runtime-config path is wired |

No deep-link intent-filter is needed for a single-entry application. A debug-only inject broadcast, if one exists, belongs in a debug source set and never on the production path.

---

## 5. Service versus UI — why the screen is not the listener

| Layer | Component | Lifecycle |
| --- | --- | --- |
| UI | Activity + Compose | May pause; the display area can show anything while reception continues |
| Transport host | Foreground service | Runs for the duration of the drive |

If the receive loop lived inside the Activity, backgrounding the UI would drop traffic — and the traffic in question is what the UI exists to display. The manifest-declared service is the lifecycle host; Compose only **observes** state.

---

## 6. Comparison with the classic phone patterns

| Classic Android pattern | Head-unit application |
| --- | --- |
| Multiple Activities, one per screen | A single Activity |
| Fragments and `FragmentManager` transactions | Compose, a display-mode value, and `AnimatedContent` |
| XML layouts and `findViewById` | Composable functions |
| `startActivity` to open a screen | A mode change driven by view-model state |
| A bound service is optional | A bound foreground service is mandatory |

---

## 7. Where the pieces live

```
app/src/main/
├── AndroidManifest.xml          ← declares the Activity, the Service and the feature
└── java/…/
    ├── App.kt                   ← Application class, object graph root
    ├── MainActivity.kt          ← starts and binds the service; setContent
    └── ui/
        ├── DisplayMode.kt
        ├── ModeViewModel.kt     ← display-area switcher and wake-on-alert
        ├── AlertViewModel.kt
        ├── screen/RootScreen.kt ← chrome plus the display area
        └── view/                ← the renderer seam and its implementations
```

Further reading: [android-automotive-os.md](android-automotive-os.md).
