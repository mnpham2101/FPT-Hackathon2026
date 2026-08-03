# IVI HMI Bring-Up — Build to God View on the AAOS Node

**The authoritative procedure for getting the IVI app from source onto the Skycraft AAOS node and proving it renders the R17 God View.** Anything about *how* the IVI APK is built, obtained from CI, installed, launched or verified belongs here; other documents link to this one rather than restating it.

- **Companion to** [deploy-walkthrough-netcheck.md](deploy-walkthrough-netcheck.md) — the template for **container** nodes (build → push to Zot → node pulls). Read it once for the platform model (blueprint, node, pin, Device, Room) and the credential table; nothing from it is repeated here.
- **Node facts** — VM artifact IDs, the `image` config block, the `ethernet` pin shape, the acceptance list — live in [node-ivi-ecu.md](node-ivi-ecu.md) and are linked, never copied. That file owns the node's *facts*; this one owns the *doing*.
- **Serves** R16 and R17 acceptance ([m1-cooperative-awareness.md §2](../m1-cooperative-awareness.md)), the R4 consumer half, and the R6 hop-3 check the Phase 0 smoke test left open.

**The one structural difference from every other node: there is no image to push and nothing to pull.** The node's VM image is the starter-pack AAOS artifact; the team's deliverable is an APK installed by hand **after** the Room is Running. That is why deploy comes before install below — the guest must exist before anything can be installed into it. The full answer, and the three routes that do *not* exist, is [§4.1](#41-how-the-apk-reaches-the-ivi-ecu-node).

```
IVI_ECU/  ──gradlew──▶  app-debug.apk  ──copy/download──▶  your machine  ──adb install──▶  AAOS guest
 (source)               (§2 or §3)                          (§3.3)           (§4.6)     in a Running Room (§4.3)
```

> ## The APK on `main` cannot be started
>
> `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/` holds the models, `MainViewModel`, `MainScreen`, `CanvasWarningView`, `IviWarningViewSeam`, `SceneCoordinateMapper` and `WarningBannerOverlay` — and **no `MainActivity`**. [AndroidManifest.xml](../../IVI_ECU/app/src/main/AndroidManifest.xml) declares no `<activity>` and no launcher entry.
>
> Today's APK therefore **installs cleanly and has no way to start**. `am start` answers `Error type 3 … Activity class does not exist`. The launcher entry lands with subtask **`16.5.5.5`** ([phase5_minh_tasks.md](../../plans/phase5_minh_tasks.md)).
>
> Until it does, §1 through §4.6 are still worth running end to end: they prove the **route** to the guest, which is the phase's biggest schedule risk (`16.5.8.3`). §4.7 onward need the app.

---

## 1. Prerequisites

### 1.1 Toolchain on the build machine

Needed only for the local build of §2. **The CI route of §3 needs none of it** — see §1.2.

| Need | Value | Why this value |
|---|---|---|
| JDK | **17**, or **21** | `compileOptions` / `jvmTarget` are Java 17 in [app/build.gradle.kts](../../IVI_ECU/app/build.gradle.kts) and CI uses Temurin 17. A JDK 21 toolchain emits the same Java 17 bytecode and builds this project. **JDK 25 fails** Kotlin/Gradle script parsing |
| Gradle | nothing to install | The wrapper pins Gradle 8.13 ([gradle-wrapper.properties](../../IVI_ECU/gradle/wrapper/gradle-wrapper.properties)) and downloads it on first run |
| Android SDK | platform **android-34** | `compileSdk = 34` |
| Build-tools | **34.0.0** or newer | Provides `aapt`, needed by §2.6 |
| Platform-tools | latest | Provides `adb`, needed by §4.5 through §4.8 |

The full Android Studio IDE is **not** required — `sdkmanager` from the Android command-line tools installs the three SDK packages above on its own.

Point Gradle at the SDK one of two ways:

- `ANDROID_HOME` in the environment, or
- a `local.properties` file beside [IVI_ECU/settings.gradle.kts](../../IVI_ECU/settings.gradle.kts) containing one line:

```
sdk.dir=C\:\\Users\\<you>\\AppData\\Local\\Android\\Sdk
```

`local.properties` is git-ignored on purpose ([.gitignore](../../.gitignore)) — it is machine-specific and must never be committed.

*Assumption — check before following §2:* that a JDK and an Android SDK are installed at all. If neither is, §3 is the faster path to an APK.

### 1.2 Platform access

Three platforms appear anywhere near this guide. **Only two of them are on the path.** Check each row before starting; a missing credential is discovered fastest here, not halfway through a deploy.

| Platform | Needed for the APK? | What you need | Used at |
|---|---|---|---|
| **GitHub Actions** | **Yes**, for the CI route | A GitHub account with read access to this repository. Optionally the [`gh` CLI](https://cli.github.com/), authenticated with `gh auth login` — it replaces every browser step in §3 | §3.2, §3.3 |
| **CarSky** | **Yes** | **Keycloak login** (email + password) for the Nydus and Devices web UIs, **and** a CarSky **API key** (`a8k_…`) for the REST calls | §4.2, §4.3, §4.4, §4.7 |
| **Zot registry** | **No** | Nothing. No `zak_…` key is used anywhere in this guide | — |

Where the two CarSky credentials come from, and how to verify one: [carsky-deploy-preflight](../../.claude/skills/carsky-deploy-preflight/SKILL.md).

**Zot is not in the path for an APK — settled, not "usually not".** The registry holds **container images**, which is what a *Container* node pulls at deploy time. The IVI node is a **Skycraft** node: it takes its VM image from the CarSky **artifact store** ([node-ivi-ecu.md § Prepare the VM artifact](node-ivi-ecu.md#prepare-the-vm-artifact-once-per-team-not-per-deploy)), and it pulls nothing else, ever. Nothing in §1–§6 runs `docker login`, `docker push`, or tags an image.

The one place Zot enters Phase 5 is a **different node**: the R4 simulator image `m1-r4-sim:latest` that the *ADA* node pulls to generate the traffic §4.8 verifies. That is the message *source*, not the APK; it has its own CI lane (`5.5.7.3`) and its own credential path ([zot-registry-api-key.md](zot-registry-api-key.md)).

### 1.3 What must already exist

This walkthrough is executable **only as far as the artifacts it consumes exist**. Check this table before following it end to end.

| Input | State on `main` | Lands with |
|---|---|---|
| An APK that can be *installed* | Present — `assembleDebug` works today | — |
| CI lane publishing `app-debug-apk` | **Present** — [phase5-ci.yml](../../.github/workflows/phase5-ci.yml) | — |
| An APK that can be *launched* | **Absent** — no `<activity>`, no `MainActivity` (see the banner above) | `16.5.5.5` |
| The R4 listener (`IVI_V2X` log lines, socket on 47300) | **Absent** | `4.5.5.2`, `4.5.3.3` |
| The dev injector (broadcast injection point I3) | **Absent** | `4.5.6.7` |
| The R4 simulator image `m1-r4-sim:latest` | **Absent** | `5.5.6.6`, `5.5.7.3` |
| A Room with a Running Skycraft node | Deployable today from the baseline blueprint | — |

---

## 2. Building on the local machine

Everything in this section runs from the `IVI_ECU/` folder. If you have no local JDK or Android SDK, skip to §3.

### 2.1 Pick the command for your host

| Host | Wrapper command | What is different |
|---|---|---|
| **Windows x86_64** | `.\gradlew.bat assembleDebug` | Nothing — the reference case |
| **Windows arm64** | `.\gradlew.bat assembleDebug` | Same command, one JDK constraint and one hard limit — §2.2 |
| **Linux** | `chmod +x gradlew && ./gradlew assembleDebug` | The wrapper script loses its executable bit on some fresh clones; `chmod +x` once fixes it |

### 2.2 Host architecture on Windows

**The APK itself is architecture-portable, and the build host's architecture is irrelevant to it.** This is worth stating plainly because the Skycraft guest is `aarch64` and the R4 simulator *image* genuinely does need `--platform linux/arm64` — the two are not the same problem, and confusing them wastes a day.

The APK is a **universal build** carrying every ABI:

| `lib/` entry inside `app-debug.apk` | Size |
|---|---|
| `arm64-v8a/libandroidx.graphics.path.so` | 10,096 B |
| `armeabi-v7a/libandroidx.graphics.path.so` | 7,252 B |
| `x86/libandroidx.graphics.path.so` | 9,284 B |
| `x86_64/libandroidx.graphics.path.so` | 10,760 B |

Everything else is DEX bytecode — nine `classes*.dex`, roughly 25 MB, compiled on the device by ART. That single native library is a **prebuilt** shipped inside an AndroidX AAR from Maven for all four ABIs; nothing is compiled for the host CPU. An identical APK therefore comes out of an x86_64 Linux runner, an Apple-silicon Mac, or an ARM64 Windows laptop, and **the `arm64-v8a` slice the AAOS guest needs is present in every one of them**. No `abiFilters`, no per-architecture build, no `--platform` flag.

Two things *do* change on an ARM64 Windows host, and one of them removes a fallback this guide would otherwise offer.

**JDK availability.** Eclipse Temurin publishes **no Windows/AArch64 build of JDK 17**; its Windows/AArch64 line starts at **21.0.5**. Install **Temurin 21** (native) rather than hunting for a native 17 — the project's Java 17 *bytecode* target is a compiler setting, not a JDK requirement. A Windows x64 JDK 17 also runs under Windows' x64 emulation layer if you need to match CI exactly. Check what you have before building:

```powershell
javac -version
```

A JRE has no `javac` and cannot run Gradle at all — that is a different failure from having the wrong version.

**No Android emulator, at all.** This is a hard platform limit, not a configuration problem:

- `sdkmanager` ships `emulator.exe` as an **x86_64 PE binary** (COFF machine `0x8664`), so it can only run under Windows' x64 emulation layer. Google publishes no ARM64 Windows emulator; the native ARM host build exists for macOS only.
- `emulator -accel-check` exits **3** with *"requires an Intel/AMD processor with virtualization extension support (Virtualization extension is not supported)"* on a Snapdragon X-series host, where `VirtualizationFirmwareEnabled` reads `False`.
- An `arm64-v8a` AVD is refused outright by an x86_64 QEMU2 host, and an `x86_64` AVD would mean software-emulating x86_64 on an ARM CPU from inside an x64-emulated process — not viable for booting AAOS.

**Consequence:** the "fall back to an AAOS emulator" escape hatch in §4.10 **does not exist on an ARM64 Windows machine**. The Skycraft guest is the only in-Room target, which is why §4.4 through §4.6 must be proven early (`16.5.8.3`). For UI-only checks a **physical Android 10+ phone over USB** works — the app declares no car-API dependency, and `adb install` does not enforce the manifest's `automotive` feature (that element filters the Play Store, not the installer). A CI runner or any x86_64 machine can still host an emulator; the constraint is local to the host, not to the project.

One cosmetic effect of a Studio-less SDK install on any host: with no NDK present, `stripDebugDebugSymbols` reports *"Unable to strip … libandroidx.graphics.path.so, packaging as is"*. The library ships unstripped — a few KB larger, functionally identical. Record the APK size from a CI run rather than from such a build.

### 2.3 Build the APK

```powershell
cd C:\Users\<you>\Documents\Work\FPT-Hackathon2026\IVI_ECU
.\gradlew.bat assembleDebug
```

Expected tail:

```
BUILD SUCCESSFUL in 7m 02s
```

A first run is the slow one — the wrapper downloads Gradle 8.13 and the build resolves AGP 8.13, Kotlin 2.2.20 and the Compose BOM. Later runs are a fraction of that.

### 2.4 Run the unit tests

Do this before shipping an APK anywhere; it is the same gate CI applies in §3.1.

```powershell
.\gradlew.bat :app:testDebugUnitTest
```

Expected: `BUILD SUCCESSFUL`, with the two committed contract tests — `R4RoundTripTest` and `R4AdditiveVersionTest` — reported green in `app/build/reports/tests/testDebugUnitTest/index.html`.

The five-module command replaces this one when the module split lands; the command table in [phase5_minh_tasks.md § Build & verification commands](../../plans/phase5_minh_tasks.md) is authoritative for which invocation is valid at which point.

### 2.5 Where the artifact lands

```
IVI_ECU/app/build/outputs/apk/debug/app-debug.apk
```

That exact path is what §2.6, §3.1 and §4.6 all refer to. Expect roughly 25 MB.

### 2.6 Check the APK is launchable

An APK with no launcher activity installs cleanly and cannot be started — the failure this check exists to catch:

```bash
"$ANDROID_HOME/build-tools/34.0.0/aapt" dump badging app/build/outputs/apk/debug/app-debug.apk | grep launchable-activity
```

| Result | Meaning |
|---|---|
| `launchable-activity: name='com.hackathon.v2x.ivi.MainActivity' …` | Launchable — §4.7 will work |
| *(no output)* | No launcher entry. **Expected on `main` today**; use this build for route-proving only |

The same check runs in CI and reports its answer as a run notice (§3.2), so a downloaded artifact never has to be inspected blind.

### 2.7 Troubleshooting the local build

| Symptom | Cause | Fix |
|---|---|---|
| `SDK location not found. Define a valid SDK location with an ANDROID_HOME environment variable or by setting the sdk.dir path in your project's local properties file` | No SDK path | §1.1 |
| `Unsupported class file major version`, or a toolchain error naming a JDK | Wrong JDK on `JAVA_HOME` | Use JDK 17 or 21; JDK 25 is not usable |
| Kotlin DSL script parse errors in `build.gradle.kts` before any compilation | JDK 25 | Same |
| `javac` not found, though `java -version` answers | A JRE, not a JDK, is on `PATH` | Install a JDK — §2.2 |
| `./gradlew: Permission denied` | Wrapper not executable after a fresh clone on Linux | `chmod +x gradlew` |
| `Build was configured to prefer settings repositories … but repository 'X' was added by build file` | A module declared its own `repositories` block | Forbidden by `FAIL_ON_PROJECT_REPOS` in [settings.gradle.kts](../../IVI_ECU/settings.gradle.kts) — remove it |
| `Unable to strip … packaging as is` | No NDK installed | Not an error; see §2.2 |

---

## 3. Building on CI

The CI route needs **no local Android SDK and no local JDK** — only a GitHub account. It is the recommended path on a machine that does not already build the app, and it is the only route that produces an APK whose unit tests are known to have passed in the same job that built it.

### 3.1 The workflow, its job and its triggers

[.github/workflows/phase5-ci.yml](../../.github/workflows/phase5-ci.yml), workflow name **`phase5-ci`**, one job **`ivi-assemble`**, runner `ubuntu-latest`.

| Trigger | Scope |
|---|---|
| `push` | **Every** push, on every branch |
| `pull_request` | Pull requests targeting `main` |

- **No path filters.** A push that touches nothing under `IVI_ECU/` still runs the lane. That matches `phase0-ci.yml` and `phase1-ci.yml`, which are equally unfiltered.
- **Concurrency cancellation.** The `concurrency` group is `phase5-ci-<ref>` with `cancel-in-progress: true`, so a newer push to the same branch cancels the older run instead of letting it finish. The group is per-ref, so `main` and a feature branch never cancel each other. A cancelled run is grey, not red, and publishes no artifact — use the newer run.
- **Timeout 30 minutes.** A cold run resolves AGP 8.13, Kotlin 2.2.20 and the Compose BOM before compiling anything; the same `assembleDebug` measures about 7 minutes warm locally. 30 bounds a hung dependency resolve without capping a legitimately slow first build. A run that hits the cap is a failure, not a slow success.

The job's steps, in order, each named as it appears in the run log:

| Step | What it does, and what it proves |
|---|---|
| `actions/checkout@v4` | Clean checkout of the pushed ref |
| `actions/setup-java@v4` — distribution `temurin`, java-version `17`, `cache: gradle` | The same JDK the app's `jvmTarget` names, matching `phase0-ci.yml`'s `ivi-unit-tests`. Gradle caching is the action's own; no separate `actions/cache` entry |
| **Unit tests (the gate for the artifact below)** — `chmod +x gradlew` then `./gradlew :app:testDebugUnitTest --no-daemon` | **The gate.** No APK leaves the workflow unless its own unit tests passed in the job that produced it |
| **Assemble the debug APK** — `./gradlew assembleDebug --no-daemon` | The debug APK builds on a clean Linux machine with no local state |
| **Record the APK size** | Emits `::notice::app-debug.apk is <N> bytes (<M> MiB)` — the number `16.5.7.1` asks to be recorded. A missing APK after a successful assemble is an `::error::` and fails the lane |
| **Report whether the APK has a launcher entry** | Runs §2.6's `aapt` check for you and emits its answer as a notice. **Reports, never fails** |
| **Upload the debug APK** — `actions/upload-artifact@v4`, name **`app-debug-apk`**, `if-no-files-found: error` | The downloadable artifact §3.3 fetches. An empty upload fails the lane rather than publishing nothing |

Two properties of this lane that look like omissions and are not:

- **Debug build only.** `app/build.gradle.kts` declares no signing config and nothing downstream consumes a release APK — the artifact installed on the guest is `app-debug.apk`. There is no release path to look for.
- **No `lint` step**, although subtask `16.5.7.1` names one. This project's lint findings have not been read, and one pre-existing finding would turn red the single lane that produces the APK. Lint is added together with `lint { abortOnError = false }` and the findings recorded, once they are known.

The Android SDK comes from the `ubuntu-latest` runner image, as `phase0-ci.yml`'s `ivi-unit-tests` lane already relies on. *Assumption:* that the runner image's SDK and licence state stay sufficient for `compileSdk 34`. If a run fails on a missing SDK component, `android-actions/setup-android` is the documented remedy (Phase 5 HLD §6.1) — add it and record that it was needed.

**Why this lane duplicates `ivi-unit-tests`.** `phase0-ci.yml`'s `ivi-unit-tests` remains the maintenance home of the IVI test invocation (`4.5.7.2` extends it to five modules). The copy here is a gate on a hand-installed artifact, not a second test lane — extend `ivi-unit-tests` when test targets change, never this step.

### 3.2 Check that the run finished and passed

Two equivalent routes. Use the browser if you have no `gh`; use `gh` if you do, because it is the only version of this section an agent can perform (§5).

**Route A — the Actions tab.**

1. GitHub → the repository → **Actions**.
2. Left sidebar → **phase5-ci**. The newest run for your branch is at the top.
3. Read the run's status icon:

   | Icon | Meaning |
   |---|---|
   | Yellow dot / spinner | Still running — no artifact yet |
   | Green check | Finished, every step passed — the artifact exists |
   | Red cross | Finished and failed — open the job and read the first red step |
   | Grey slash | Cancelled, because a newer push to the same branch superseded it (§3.1). Not a failure; use the newer run |

4. Open the run → job **`ivi-assemble`**. The job's **Annotations** carry the two notices worth reading before downloading anything (below).
5. Scroll to the bottom of the run summary page. The **Artifacts** section lists **`app-debug-apk`**. An artifact appears only after the run finishes.

**Route B — the `gh` CLI.** Run from anywhere inside the repository clone:

```bash
gh run list --workflow phase5-ci --branch <your-branch> --limit 5
```

Expected — one line per run, newest first, with the run ID in its own column:

```
STATUS  TITLE                      WORKFLOW    BRANCH  EVENT  ID          ELAPSED  AGE
✓       ci: add phase5-ci …        phase5-ci   main    push   1234567890  6m2s     10m
```

The `STATUS` column is the same four states as the table above: `✓` passed, `X` failed, `*` in progress, `-` cancelled.

To block until a still-running run finishes, and get a non-zero exit code if it failed:

```bash
gh run watch <run-id> --exit-status
```

To see which step failed without opening a browser:

```bash
gh run view <run-id>              # per-step status for every job
gh run view <run-id> --log-failed # only the failing step's log
```

**The two notices, whichever route you took.** They are the reason a downloaded APK never has to be inspected blind:

| Notice text | Means |
|---|---|
| `app-debug.apk is <N> bytes (<M> MiB)` | The size to record for `16.5.7.1`. Expect roughly 25 MB |
| `the APK declares a launcher activity and can be started with 'am start'` | §4.7 will work |
| `the APK declares NO launcher activity - it installs and cannot be started (the entry lands with 16.5.5.5)` | **Expected on `main` today.** The build is good for route-proving only |
| `no aapt found in the runner image's build-tools - launcher check skipped` | The runner image had no `aapt`; the check did not run. Fall back to §2.6 locally on the downloaded file |

In the browser these appear as **Annotations** on the run summary. From the CLI they are lines in the step's log — `gh run view <run-id> --log` contains them verbatim.

### 3.3 Get the APK off CI

**Route A — the browser.**

1. On the run summary page, click **`app-debug-apk`** under **Artifacts**. The browser downloads `app-debug-apk.zip` to your usual downloads folder.
2. **Unzip it.** GitHub always wraps an artifact in a zip; `adb install` on the zip fails with `INSTALL_PARSE_FAILED_NOT_APK`. The single file inside is `app-debug.apk`.
3. Record its SHA-256 and size — `adb install` reports nothing about provenance, so this is the only way to prove later that the build on the guest is the build you fetched.

```powershell
Expand-Archive .\app-debug-apk.zip -DestinationPath .\apk
Get-FileHash -Algorithm SHA256 .\apk\app-debug.apk
```

**Route B — the `gh` CLI.** One command, and **no unzip step** — `gh` extracts the artifact for you:

```bash
gh run download <run-id> --name app-debug-apk --dir ./apk
```

The file lands at `./apk/app-debug.apk`. Omitting `--dir` extracts into the current directory. Omitting `--run-id` makes `gh` prompt from the list of recent runs.

**The artifact name `app-debug-apk` is contractually stable.** It is fixed by the workflow, and the workflow's own comment forbids renaming it *because this guide tells a human to download exactly that name*. A rename breaks this guide, not only the workflow.

**There is no route from CI to the platform.** Do not go looking for one — §4.1 states why, and what the only ingress is.

### 3.4 Troubleshooting the CI route

| Symptom | Meaning | Action |
|---|---|---|
| No **phase5-ci** entry in the Actions sidebar | The workflow file is not on the branch you are looking at | Check out / push the branch carrying `.github/workflows/phase5-ci.yml` |
| Run red at *Unit tests (the gate for the artifact below)* | A committed test broke — the gate did its job | Fix the test; no artifact is published, by design |
| Run red at *Assemble the debug APK* with an SDK or licence error | Runner-image SDK insufficient | Add `android-actions/setup-android` (§3.1) and record that it was needed |
| Run red at *Record the APK size* with `is missing after a successful assembleDebug` | The assemble step reported success but produced no file at the expected path | The output path changed; re-check `app/build.gradle.kts` against §2.5 |
| Run red after exactly 30 minutes | The `timeout-minutes: 30` cap (§3.1) | Almost always a hung dependency resolve; re-run the job |
| Run green, no **Artifacts** section | Looking at a still-running or cancelled run | Wait for the green check, or open the newest run |
| Grey slash on the run you were watching | A newer push cancelled it (§3.1) | Not a failure — use the newer run |
| `gh: To use GitHub CLI in a GitHub Actions workflow, set the GH_TOKEN` or an auth prompt | `gh` is not authenticated on this machine | `gh auth login`, or use Route A |
| `INSTALL_PARSE_FAILED_NOT_APK` later, at §4.6 | The zip was installed instead of the APK inside it | §3.3 Route A, step 2 |

---

## 4. Deploying on CarSky

### 4.1 How the APK reaches the IVI ECU node

**By hand, over ADB, from a machine holding the file. There is no other way, and nothing pulls it.**

State this up front because the obvious question — *"how do I export the APK so the CarSky IVI node can pull it?"* — rests on a false premise. A **Container** node names an image and the platform pulls it at deploy time; that is the model [deploy-walkthrough-netcheck.md](deploy-walkthrough-netcheck.md) teaches. The IVI node is a **Skycraft** node and does not work that way:

| Candidate route | Why it does not exist |
|---|---|
| **Zot registry** | Serves container images to Container nodes. The IVI node pulls no container image at all (§1.2) |
| **The CarSky artifact store** | Holds the AAOS **VM image** the node boots ([node-ivi-ecu.md](node-ivi-ecu.md#prepare-the-vm-artifact-once-per-team-not-per-deploy)). The APK is deliberately not baked into that image, and the artifact is shared team-wide — republishing it per APK build is not the model |
| **The REST API** | [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md) enumerates the live routes; none of them accepts a file upload into a guest |
| **The CI workflow** | [phase5-ci.yml](../../.github/workflows/phase5-ci.yml) publishes a run artifact and stops there. Its header says so explicitly: there is no path from the workflow to the platform |

The actual sequence, and the reason install comes after deploy:

1. Build the APK — locally (§2) or on CI (§3) — so a machine you control holds `app-debug.apk`.
2. Deploy the blueprint (§4.3) and wait for the Skycraft node to reach `Running`. **The guest must exist before anything can be installed into it.**
3. Obtain an ADB endpoint to that guest (§4.4) and install over it (§4.6).

Every rebuild repeats steps 1 and 3 only — the Room stays up, and `adb install -r` replaces the app in place.

### 4.2 Configure the blueprint and its IVI node

| Blueprint | Use when |
|---|---|
| The 5-node baseline ([carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)) | Full-chain work — bench, V2X, ADA, IVI, bridge |
| The 3-node mini-blueprint — bridge + ADA + IVI | IVI work alone: fewer nodes, faster deploy, leaves the second Room slot free — §4.11 |

Three things about the IVI node decide whether the deploy is even accepted, and all three are **facts owned by [node-ivi-ecu.md](node-ivi-ecu.md)** — read them there, do not retype them here:

| What | Where it is defined | What can go wrong |
|---|---|---|
| The VM image artifact IDs | [§ Prepare the VM artifact](node-ivi-ecu.md#prepare-the-vm-artifact-once-per-team-not-per-deploy) | Uploading a second copy of an artifact that already exists |
| The Skycraft `image` config block | [§ Blueprint node config](node-ivi-ecu.md#blueprint-node-config) | Its absence gets the whole deploy **rejected outright**, with the exact message quoted there. On a clone the block is already correct — leave it alone. On a hand-authored or imported node it is usually missing |
| The `ethernet` pin shape and address | [§ Pins](node-ivi-ecu.md#pins) | Its absence leaves the node with no network at all |

**The `ethernet` pin is a manual UI step.** Neither the REST API nor Nydus **Import from File** can create an `ETHERNET` pin ([carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)) — an import silently drops it. A **clone keeps its pins**; an import does not. Prefer cloning. If the pin is missing, draw it by hand on the Nydus canvas and wire it to the Ethernet Bridge node, same-type only (`ethernet ↔ ethernet`).

**Verify by reading the stored config back**, not by trusting the Inspector's truncated fields:

```bash
export CS=https://hackathon-2.carsky.io
curl -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints/{id}
```

Expect one `ETHERNET` / `OUTPUT` pin on the IVI node at the address [node-ivi-ecu.md § Pins](node-ivi-ecu.md#pins) fixes, wired to the bridge's single `INPUT` pin.

**Record the Skycraft display config while you are in that read-back.** The Inspector's CONFIGURATION group on a Skycraft node carries fields no other node type has, and §4.7 needs two of them:

| Field | Why you need it |
|---|---|
| **Part Prefix** | Auto-names every "part" of the node — `<prefix>-screen`, `<prefix>-audio`, `<prefix>-logcat`, `<prefix>-adb`. §4.4 and §4.7 select parts by these names |
| **Display Width / Height / DPI** | The virtual screen resolution the Screen widget shows. The committed R16 previews are drawn for 1280×720 ([MainScreen.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt) preview names) |
| **GPU Backend** | The virtual graphics driver given to the VM |

*Unverified:* the actual values on our IVI node. Read them off the read-back and write them down before you need them — **do not assume 1280×720**.

### 4.3 Deploy the blueprint

1. In Nydus, open the blueprint (the original, **never** the `<name>-deploy` snapshot that deploying creates).
2. Click empty canvas → the blueprint Inspector → **New Deployment**.
3. Pick an **existing Device** from the dropdown — the Kubernetes resource pool, not an ECU. `+ Create new device` is unnecessary and eats into the 2-concurrent-deployment budget.
4. **Deploy**, then wait until every node badge reads `Running` with restart count 0.

**The Skycraft node is the slowest to reach Running** — expect it to lag the containers.

Poll it without the browser if you prefer:

```bash
curl -H "Authorization: Bearer $KEY" $CS/api/v1/deployments/{roomId}/nodes
```

Each entry carries `{displayName, name, nodeType, phase, message}`. `name` is the **`nodeKey`** every route below needs; `{roomId}` is the Device's id. Deploy-dialog details and the two-Room budget: [deploy-walkthrough-netcheck.md § M9](deploy-walkthrough-netcheck.md). Diagnosis when a node hangs: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md).

### 4.4 Get an ADB endpoint

> **This step has never been proven on this deployment.** It is the phase's biggest schedule risk (`16.5.8.3`), and its outcome decides whether the R16/R17 evidence is in-Room or device-only. Run it early, against today's APK, before the app is finished.

The platform states the shape of the answer plainly: *"The Skycraft pod exposes ADB on a fixed port; use Rework's device panel or CarSky Gateway ADB tunnel to connect."* Three candidate routes follow from that, strongest first. **Record which one actually worked.**

| # | Route | State |
|---|---|---|
| 1 | `GET /api/v1/deployments/{roomId}/adb-tunnel` — *"Get ADB tunnel command info for Skycraft node."* Then `adb connect localhost:<port>` and use local `adb` normally | Documented by the platform ([Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html) § API & MCP Tools). **Unverified on `hackathon-2`** |
| 2 | Devices panel → the device the deployment created → **Connect** → `+` → **ADB** widget, with the ADB part set to `<prefix>-adb` | Documented; gives an in-browser shell (prompt `trout_arm64:/ $`). The platform describes it as usable to *"run commands, install APKs, view logs"* — but it exposes **no file transfer**, so it cannot take a local `.apk` unless the file is already reachable from the guest |
| 3 | `POST /api/v1/deployments/{roomId}/adb-exec/{nodeKey}` — *"Run one-shot ADB command."* | Documented. **Unverified**, and the same file-transfer limitation as #2 |

Two routes that are **not** available, so nobody spends time on them:

- The MCP tunnel tool `vm_tunnel_open` needs CarSky's own MCP server package, which this repo does not carry ([carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)).
- `POST /api/v1/vms/{roomId}/{nodeKey}/shell` is **known-502** on this deployment and must not be the primary plan.

Probe the 502 pair anyway while you are here — `…/shell` and `…/screenshot`. A live `screenshot` route is a **second evidence path** for §4.9 that does not depend on ADB at all, and it is a cheap check:

```bash
curl -s -o /dev/null -w '%{http_code}\n' -H "Authorization: Bearer $KEY" \
  $CS/api/v1/vms/{roomId}/{nodeKey}/screenshot
```

### 4.5 Connect and check the guest

```bash
adb connect <skycraft-adb-endpoint>
adb devices
```

Expected:

```
List of devices attached
<endpoint>   device
```

`offline` or an empty list means the endpoint is wrong or the tunnel is not up — go to §4.10 rather than retrying blind.

Two guest properties decide whether the APK can be installed at all, and both are unknown until you read them:

```bash
adb shell getprop ro.build.version.sdk
adb shell getprop ro.build.version.release
adb shell pm list features | grep automotive
```

| Reading | Requirement | If it fails |
|---|---|---|
| `ro.build.version.sdk` | ≥ **29** (`minSdk 29`) | Install is rejected outright; the APK's `minSdk` would have to change |
| `android.hardware.type.automotive` present | The manifest declares it `required="true"` | A non-automotive **guest** refuses the install. (A phone over USB does not — `adb install` does not enforce the feature; see §2.2) |

### 4.6 Install the APK

```bash
adb install -r app-debug.apk
adb shell pm list packages | grep hackathon
adb shell pm path com.hackathon.v2x.ivi
```

Expected:

```
Success
package:com.hackathon.v2x.ivi
package:/data/app/~~…/com.hackathon.v2x.ivi-…/base.apk
```

`-r` reinstalls over an existing copy and keeps its data — use it on every reinstall, not only the first.

### 4.7 Open the screen and launch the app

The display is a **widget in the Devices panel**, not something in the Deployment Viewer.

1. **Devices** on the DockBar. The deployment auto-creates its device; the platform marks such an entry `🚀 Started pack-deploy`. Do not delete it by hand — stop the deployment instead.
2. **Connect** (the button reads **Switch** if another device session is already open). The dot beside the name must be green.
3. `+` → **Screen** from the widget catalog.
4. In the widget's Inspector, set the parts from the node's Part Prefix (§4.2):
   - **Video Part** → `<prefix>-screen`
   - **Touch Part** / **Keyboard Part** → the corresponding parts, or clicks in the browser never reach the guest
   - **Recorder Part** → set it when you want the run recorded; the clip appears under **Videos** after recording stops, downloadable as `.mp4`
5. Add two more widgets now — they are what §4.8 reads:
   - **Log**, source part `<prefix>-logcat` — the guest's logcat in the browser
   - **ADB**, part `<prefix>-adb` — a shell beside the screen

Expected: the AAOS screen streaming live in the Stage, and clicking on it acting like a touch.

**Black screen?** Per the platform's own FAQ: check that **Video Part** names the part the node actually publishes (same prefix as its Part Prefix); if the part is right, **Disconnect** then **Connect** to re-establish the WebRTC session.

*Unverified:* whether the guest's display ever sleeps in a Room. If the stream is up but the guest looks asleep, `adb shell input keyevent KEYCODE_WAKEUP` is the standard AOSP wake command — untested against this guest.

Then launch:

```bash
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity
```

To override the R4 port for one launch without rebuilding (HLD decision D10):

```bash
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity --ei r4_port 47301
```

Expected on the screen — the R16 layout from [MainScreen.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt):

- A central **Display Area**, flanked by the button areas: **Home**, **Apps**, **Settings** on one side, mode labels `WARNING` / `HOME` / `APPS` / `SETTINGS` on the other.
- A bottom status bar reading `MODE: <mode>` and a `V2X LINK: …` indicator.
- Tapping a side button changes what the Display Area shows.

**Record the boot-to-listener time while the app starts for the first time.** Two wall-clock deltas: guest boot → launcher, and launch → the first `[LINK] state=bound` line. Their sum is the floor for the bench's start delay, and Phase 5 is the only phase that can measure it (`16.5.9.2`).

### 4.8 Verify the HMI and the logging

The claim under test is one chain, not a set of independent checks:

```
ADA node sends an R4 datagram  ──▶  IVI guest receives it  ──▶  an event is raised in the app  ──▶  the HMI switches to the Warning View
       CarSky node log                    IVI_V2X log                     IVI_V2X log                        Screen widget
        [TX] … 47300                    [RX] type=warning              [UI] mode=WarningView             the God View, drawn
```

Every link has its own observable, and they come from **two different log surfaces**. Both are needed: the display alone does not prove where the data came from, and the logs alone do not prove anything rendered.

| Surface | How to read it | Carries |
|---|---|---|
| **CarSky node log** (the ADA container) | Deployment Viewer → the ADA node → **View Log**; or `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user` — **`container` is mandatory**, omitting it returns 500 | The producer's `[TX]` lines, and `[CAP]` tcpdump lines when the node has `NET_RAW` |
| **Guest logcat** (the IVI app) | The **Log** widget on part `<prefix>-logcat`, or `adb logcat -s IVI_V2X` | Everything the app does: `[LINK]`, `[RX]`, `[DROP]`, `[UI]` |

*Unverified:* every `IVI_V2X` line quoted below is the shape **designed** in the Phase 5 HLD §5.4, not yet built. Until the listener lands (`4.5.3.3`, `4.5.5.2`), the guest logcat carries nothing on this tag.

Work up the ladder — each rung needs less to exist than the one below it, so start at the highest rung your build supports.

#### V1 — the socket is bound

| | |
|---|---|
| **Feed** | Nothing; launching the app is enough |
| **Correct** | `[LINK] state=bound port=47300` on `IVI_V2X`, and the bottom status bar reading `BOUND :47300` |
| **Incorrect** | No `[LINK]` line at all (listener absent or not started), or a bind error followed by rebind attempts (port already taken — relaunch with `--ei r4_port`) |

#### V2 — a datagram reaches the guest (works before the simulator exists)

| | |
|---|---|
| **Feed** | Point the ADA node at the IVI: image `m1-netcheck:latest`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` — the **probe config** of subtask `5.5.8.2`. The ADA node's **View Log** shows `[TX] … relayed to 10.99.0.13:47300` |
| **Correct** | `[DROP] reason=malformed bytes=… preview="seq=…"` on `IVI_V2X`, one per datagram, and the app keeps running. netcheck's payload is not JSON, so a drop **is** the pass — it proves the socket, the bridge hop and the loop's survival |
| **Incorrect** | ADA logs `[TX]` but the IVI logs nothing → the datagram is not arriving; re-check the pin address and the port |
| **Closes** | R6 hop 3 — the check Phase 0 could only make indirectly ([deploy-walkthrough-netcheck.md § Checking IVI RX traffic](deploy-walkthrough-netcheck.md)) |

#### V3 — the UI comes up, with no network at all

| | |
|---|---|
| **Feed** | `adb shell am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample r4-warning` |
| **Correct** | The Display Area switches to the Warning View **by itself**, drawing the God View |
| **Incorrect** | `Broadcast completed: result=0` with no UI change → wrong build type: the injector exists in the **debug** build only, by design |

#### V4 — a real R4 message: the whole chain (R16 + R17 acceptance)

| | |
|---|---|
| **Feed** | ADA node → the **evidence config**: image `m1-r4-sim:latest`, `command: ["./entrypoint.sh"]` (relative), `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300`, `R4_SCENARIO=/app/scenarios/approach.json`, `R4_RATE_HZ=1`, `START_DELAY_S=20` |
| **Link 1 — sent** | ADA **View Log**: `[TX] step=… type=warning bytes=… → 10.99.0.13:47300`, at ~1 Hz |
| **Link 2 — received & parsed** | `IVI_V2X`: one `[RX] type=warning bytes=… from=10.99.0.12:… warningType=nlos_obstruction risk=… cSource=v2x_relayed cPos=(…)` per datagram. The fields after `bytes=` are read off the **parsed** message, so this line is the proof that the R4 JSON decoded into the typed model and its R3 `object` snapshot |
| **Link 3 — event raised** | `IVI_V2X`: `[UI] mode=WarningView cause=warning` — the app changed state because of the message, not because anyone tapped |
| **Link 4 — displayed** | Display Area switches itself to the Warning View. `EGO` and `B` drawn solid; **C dashed** with a pulsing risk glow and the badge `[V2X] C · <d> m · RISK: HIGH`; connector labels `d_AB = <n> m` and `d_AC ≈ <n> m`. The scenario's first step carries `geometry.vehicleC: null` and must render **without C and without a crash or placeholder** |
| **Incorrect** | A yellow **`[? UNKNOWN SOURCE]`** marker where ghost C should be. The provenance guard tripped: the scene reached the renderer without a `v2x_relayed` snapshot. On `approach.json` this is a **blocking defect**, not a display quirk |
| **Also check** | Let the stream stop: the view times out to Idle and the previous mode is restored — `[UI] mode=HomeView cause=timeout` |

`cSource=v2x_relayed` on every rendered warning **is the R19 claim in text**: the recording shows ghost C, and the log proves every frame of it came from relayed data.

#### V5 — degradation, the guard, and loop survival (R4 acceptance)

Switch the ADA node to `R4_SCENARIO=/app/scenarios/degrade.json`.

| Case in the scenario | Correct result | Incorrect result |
|---|---|---|
| Unknown `warningType` + `schemaVersion: 2` + a junk field | A generic warning renders; logcat shows the wire value **preserved** (`warningType=slippery_road`) and one schema-version-ahead notice | `FATAL EXCEPTION`, or the type rewritten to `unknown` — forbidden by HLD decision D4 |
| `object.source: "own_sensor"` | The guard **trips**: yellow `[? UNKNOWN SOURCE]` marker and an ERROR line on `IVI_V2X`. Here the trip is the pass | Ghost C drawn normally → the R19 provenance wiring is broken |
| A raw non-JSON step | `[DROP] reason=malformed …`, and **the next valid warning still renders** | The app stops rendering after the bad message |

### 4.9 Capture the evidence

- **Screen recording:** set **Recorder Part** on the Screen widget *before* the run; the clip lands under **Videos** and downloads as `.mp4`. Videos record at the screen's native resolution, so files are large.
- **Screenshots:** from the Screen widget. If the REST route `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` answered in §4.4, it is a second, scriptable evidence path.
- **Guest log excerpt:** `adb logcat -s IVI_V2X` — the `[RX] … cSource=v2x_relayed` lines are what back the recording in text.
- **Producer log excerpt:** the ADA node's `[TX]` lines, from **View Log** or the logs route.
- Record every result in the phase's run record, as [phase0-smoke-test-run.md](../../plans/doc/phase0-smoke-test-run.md) does for the smoke test.

### 4.10 Troubleshooting the deploy and install

| Symptom | Meaning | Action |
|---|---|---|
| Deploy rejected: `skycraft requires 'image' config with VM image artifact details` | The IVI node has no `image` block | [node-ivi-ecu.md § Blueprint node config](node-ivi-ecu.md#blueprint-node-config); §4.2 |
| `failed to connect` / device `offline` | The endpoint or tunnel is wrong | Try the next route in §4.4; do not retry the same one |
| `INSTALL_FAILED_OLDER_SDK` | Guest below API 29 (§4.5) | Blocking finding — escalate; the in-Room plan changes |
| `INSTALL_FAILED_MISSING_SHARED_LIBRARY`, or a feature error naming `automotive` | Not an automotive system image | Same — escalate |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | An older build with a different signature is installed | `adb uninstall com.hackathon.v2x.ivi`, then install again |
| `INSTALL_PARSE_FAILED_NOT_APK` | The downloaded zip was installed, not the APK inside it | §3.3 |
| `Error type 3 … Activity class {…/.MainActivity} does not exist` | The installed APK has no launcher entry | §2.6, and the banner at the top — it lands with `16.5.5.5` |
| App starts, then the screen returns to the launcher | Crash on start | Read `adb logcat` **unfiltered** before filtering by tag |
| `V2X LINK: STANDBY` never changes | The status bar is still the committed placeholder | It is bound to the real link state by subtask `17.5.5.6` |
| Node stuck in `Provisioning` | Almost always an image that cannot be pulled | Only affects container nodes; check the ADA node's image field. [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md) |
| No route to the guest works at all | ADB to the guest is unreachable on this deployment | Fall back to a **physical Android 10+ device** over USB for §4.7–§4.8 and **record that the evidence is device evidence, not in-Room evidence**. On an x86_64 host an Android Automotive emulator is an alternative; on ARM64 Windows it is not (§2.2) |

### 4.11 The mini-blueprint route

For IVI-only work, a 3-node topology — Ethernet Bridge + ADA node + IVI Skycraft node — deploys faster, has fewer variables, and leaves the second Room slot free for the comms track. **The mechanics are exactly §4.2 through §4.10; only the composition differs**, so nothing above is repeated here.

- **Design and rationale:** [phase5-mini-blueprint.md](../../IVI_ECU/doc/research_notes/phase5-mini-blueprint.md). The sanctioned deployment procedure lands at `requirements/car-sky-guide/phase5-mini-blueprint-deploy.md` with subtask `5.5.8.1`.
- **Creation route — clone, then delete.** Clone the known-good baseline, rename it, then delete the Bench and V2X nodes on the canvas. Cloning is the only route that preserves `ethernet` pins (§4.2); a blueprint built from scratch by script has no network at all. Deleting a node removes its pin and edge; the ADA and IVI pins are untouched.
- **Do not import** a hand-authored blueprint JSON in place of this. Every such attempt so far omitted the Skycraft `image` block, which gets the deploy rejected outright.
- **The ADA node is the only node that is reconfigured** — probe config (`m1-netcheck:latest`) before the simulator image exists, evidence config (`m1-r4-sim:latest`) after. Both are in §4.8. Addresses, the `47300` port and the pin shapes stay at the baseline values, so the Phase 6 switch to the real ADA image is an image swap with no node-config edit.

### 4.12 Tear down

**Delete Deployment** when finished. Only two Rooms may run at once and the comms track needs the slot; the blueprint itself is kept and redeployable.

---

## 5. Work division between AI and human

The split is not a preference — it follows from what an agent can reach. An agent can run CLI tools and authenticated REST calls; it cannot use the Nydus canvas, a browser download, or its own eyes.

| Action | AI / Human | Description |
|---|---|---|
| [Trigger a CI build](#31-the-workflow-its-job-and-its-triggers) | AI | Push a commit; the lane runs unfiltered on every push |
| [Confirm the run passed](#32-check-that-the-run-finished-and-passed) | Human | Actions web UI; an agent session holds no GitHub token |
| [Download `app-debug-apk`](#33-get-the-apk-off-ci) | Human | Artifact download needs an authenticated token; unzip before use |
| [Configure the blueprint and its pins](#42-configure-the-blueprint-and-its-ivi-node) | Human | Nydus canvas — REST cannot create `ETHERNET` pins |
| [Read the stored config back](#42-configure-the-blueprint-and-its-ivi-node) | AI | `GET /api/v1/blueprints/{id}`; also captures the Part Prefix and display fields |
| [Deploy the blueprint](#43-deploy-the-blueprint) | Human | **New Deployment** dialog; picking the Device is a human call |
| [Poll node phases until Running](#43-deploy-the-blueprint) | AI | `GET /api/v1/deployments/{roomId}/nodes`; also yields each `nodeKey` |
| [Obtain the ADB endpoint](#44-get-an-adb-endpoint) | Human | Rework device panel or the Gateway tunnel — both are browser surfaces |
| [Probe the 502 REST routes](#44-get-an-adb-endpoint) | AI | One `curl` each for `…/shell` and `…/screenshot` |
| [Connect and read guest properties](#45-connect-and-check-the-guest) | AI | `adb connect`, `adb devices`, `getprop`, `pm list features` |
| [Install the APK](#46-install-the-apk) | AI | `adb install -r`, then `pm path` to confirm |
| [Open the Screen, Log and ADB widgets](#47-open-the-screen-and-launch-the-app) | Human | Devices panel widgets and their part fields |
| [Launch the app](#47-open-the-screen-and-launch-the-app) | AI | `adb shell am start`, with the optional port override |
| [Configure the ADA node's feed](#48-verify-the-hmi-and-the-logging) | Human | Node Inspector image and env fields, then redeploy |
| [Read the two log surfaces](#48-verify-the-hmi-and-the-logging) | AI | `adb logcat -s IVI_V2X` and the node logs route |
| [Confirm the display switched](#48-verify-the-hmi-and-the-logging) | Human | A visual judgement no log line replaces |
| [Record the screen](#49-capture-the-evidence) | Human | Recorder Part, then download the clip from **Videos** |
| [Tear the Room down](#412-tear-down) | Human | **Delete Deployment**; releases one of the two Room slots |

Two qualifications on the rows above, neither of which changes the default:

- **Rows 2 and 3 flip to AI** on a machine with an authenticated `gh` CLI — Route B of §3.2 and §3.3 needs no browser. Without `gh auth login`, they stay human.
- **The local build of §2 is the AI-side alternative to the first three rows**: an agent with a JDK and an Android SDK produces the same APK without touching a browser at all.

---

## 6. Expected outputs and acceptance

Four observables prove the phase, and each one closes a specific clause of R16 or R17. Three are text and one is visual; none substitutes for another.

| # | Proof | Where it appears | Closes |
|---|---|---|---|
| 1 | **Incoming R4 message from the ADA ECU** — one `[RX] type=warning bytes=… from=10.99.0.12:…` per datagram | `IVI_V2X` on the guest, corroborated by the ADA node's `[TX]` line | R4 (the consumer half) and **R6 hop 3** — the check Phase 0 could only make indirectly |
| 2 | **The parsed TrackedObject data model** — the same `[RX]` line's `warningType=`, `risk=`, `cSource=` and `cPos=` fields, read off the decoded message and its R3 `object` snapshot | `IVI_V2X` | R4 acceptance: the consumer parses the contract, and an unknown `warningType` degrades gracefully rather than crashing (§4.8 V5, first row) |
| 3 | **The event raised in the IVI ECU** — `[UI] mode=WarningView cause=warning`, with `cause=warning` and not `cause=user` | `IVI_V2X` | R16 acceptance: *"an R4 warning brings the warning view up in the Display area"* — the message, not a tap, caused the switch |
| 4 | **The HMI switched to the warning display** — the Display Area drawing the God View: ego and B solid, ghost C dashed with its risk glow and `[V2X]` badge | Screen widget, captured as a recording or screenshot | R16 acceptance (the warning view is shown in the Display area) and **R17 acceptance** (the three vehicles, ghost C sourced only from `v2x_relayed`, 2D delivered) |

Two further observations complete R16 and R17 rather than repeating the four above:

| Observation | Closes |
|---|---|
| Tapping **Home / Apps / Settings** changes what the Display Area shows, and `[UI] mode=… cause=user` follows | R16: *"the button/app areas switch what the Display area shows"* |
| `cSource=v2x_relayed` on **every** rendered warning, and the guard tripping to `[? UNKNOWN SOURCE]` on an `own_sensor` message | R17: ghost C sourced **only** from `v2x_relayed`. This pair is also the IVI-side half of the R19 definition of done |

Optional paths — the separate app woken by an ADA message, and 3D through the view seam — are not M1 deliverables. Record them as not built rather than leaving the boxes ambiguous.

### 6.1 Unverified points — confirm before relying on them

Everything below is stated in the platform documentation or follows from the committed code, but has **not** been observed working on this deployment. A step that depends on one of them can fail without this guide being wrong.

| # | Point | Where it bites |
|---|---|---|
| 1 | ADB reach to the Skycraft guest, by any route | §4.4 — the whole in-Room evidence plan |
| 2 | `GET /api/v1/deployments/{roomId}/adb-tunnel` and `POST /…/adb-exec/{nodeKey}` answering on `hackathon-2` | §4.4 |
| 3 | The AAOS guest's API level and its `automotive` feature | §4.5 — either one blocks the install |
| 4 | The IVI node's real Part Prefix, display size and GPU backend | §4.2, §4.7 — the Screen, Log and ADB widgets select parts by these names |
| 5 | Whether the guest display sleeps, and that `KEYCODE_WAKEUP` wakes it | §4.7 |
| 6 | A JDK and an Android SDK being present on the build host | §1.1, §2 — §3 is the route that needs neither |
| 7 | `…/screenshot` and `…/shell` still answering 502 | §4.4, §4.9 |
| 8 | Every expected `IVI_V2X` log line and the bound-link indicator — **designed in the HLD, not yet built** | §4.7, §4.8, §6 |
| 9 | The `ubuntu-latest` runner image's Android SDK and licence state staying sufficient for `compileSdk 34` | §3.1 |
| 10 | Whether the ADB widget can install an APK in practice — the platform says it can, but it exposes no file transfer | §4.4 route #2 |
