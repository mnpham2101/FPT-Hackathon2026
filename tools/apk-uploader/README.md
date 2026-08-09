# apk-uploader

Installs `app-debug.apk` into the IVI-ECU Skycraft (AAOS) node of a deployed Room, over the organizers' `reach-backend` ADB tunnel, and reports whether it took.

Usage only. The procedure, the prerequisites and what to do when a step fails are in [apk-deploy.md](../../documents/Delivery/apk-deploy.md).

## Tools

| Tool | OS | What it is |
|---|---|---|
| `INSTALL-IVI-APK.cmd` | Windows | Double-clickable wrapper around `install-ivi-apk.ps1` |
| `install-ivi-apk.ps1` | Windows PowerShell 5.1 (x64 and ARM64) | The installer |
| `install-ivi-apk.sh` | Linux, macOS, Git Bash | The same installer, same flags in `--kebab-case` |
| `reach_be/reach/reach-backend.exe` | Windows x64 (runs emulated on ARM64) | The tunnel CLI the installer starts |
| `reach_be/reach/reach-backend` | Linux, macOS | The tunnel CLI, other hosts |

The two scripts are one tool for two hosts and must stay behaviourally identical.

`reach_be/` is **git-ignored and will not arrive with a clone** — unpack the organizers' zip into `tools/apk-uploader/` so the paths above exist.

## 1 · Install the APK

Put the APK at `tools/apk-uploader/app-debug.apk` and the `a8k_` tunnel token at `secrets/reach-adb-token-ivi.txt`, then run from the repo root:

```powershell
.\tools\apk-uploader\INSTALL-IVI-APK.cmd
```

```bash
./tools/apk-uploader/install-ivi-apk.sh
```

| Flag | Effect |
|---|---|
| `-KeepTunnel` / `--keep-tunnel` | Leave the tunnel up afterwards, so `adb` stays usable |
| `-CloseTunnel` / `--close-tunnel` | Close the tunnel on exit **even if this run did not open it** — the only way the script clears one inherited from an earlier run |
| `-SkipInstall` / `--skip-install` | Verify only — no reinstall |
| `-SkipNetworkFix` / `--skip-network-fix` | Do not put the guest on the Room subnet |
| `-Token` / `--token` | Use this `a8k_` token instead of the file |
| `-Apk` / `--apk` | Install a different APK |
| `-Port` / `--port` | Tunnel port; default 5555 |
| `-IviAddr` / `--ivi-addr` | The IVI node's Room address; default `10.99.0.13` |
| `-RoomGateway` / `--room-gateway` | Default route inside the guest, for an off-subnet producer |

Exit 0 installed, 1 a hard stop, 2 a usage error.

## 2 · Verify the APK is installed

The run ends in a checklist. `package installed` is the row that answers this one:

```
[x] package installed     com.hackathon.v2x.ivi 1.0
[x] process running       pid 4711
[x] MainActivity up       topResumedActivity
[x] window focused        holds input focus
[x] screen awake          mWakefulness=Awake
```

To re-check without reinstalling:

```powershell
.\tools\apk-uploader\INSTALL-IVI-APK.cmd -SkipInstall
```

By hand, with the tunnel up:

```powershell
adb -s localhost:5555 shell pm list packages | Select-String hackathon
```

Installed reads `package:com.hackathon.v2x.ivi`.

## Folder

`app-debug.apk` is the artifact to install. `logs/` holds each run's evidence logcat and `test-report/` the collected run folders; both are git-ignored. `secrets/` is at the repo root, git-ignored, and its contents are never printed or committed.
