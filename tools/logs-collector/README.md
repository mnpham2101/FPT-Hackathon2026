# logs-collector

Pulls every node log off a deployed CarSky Room over REST, and — where the Room has a Skycraft VM — the guest's logcat, crash buffer, sockets and interfaces over ADB, into `test-report/<run>/`. Read-only: it collects, it never deploys or installs.

Usage only. The procedure, the checks each file answers, and what to do when a row fails are in [testing-guide.md](../../documents/Delivery/Test-Guides/testing-guide.md).

## Tools

| Tool | OS | What it is |
|---|---|---|
| `COLLECT-LOGS.cmd` | Windows | Double-clickable wrapper around `Collect-Logs.ps1` |
| `Collect-Logs.ps1` | Windows PowerShell 5.1 | The collector |
| `collect-logs.sh` | Linux, macOS, Git Bash | The same collector, same flags in `--kebab-case` |

The two scripts are one tool for two hosts and must stay behaviourally identical.

## 1 · Run it

**On Windows, run the `.cmd`, not the `.ps1`.** A fresh Windows blocks `.ps1` files from running — `.\Collect-Logs.ps1` fails with *"running scripts is disabled on this system"* (`UnauthorizedAccess`). The `.cmd` wrapper carries `-ExecutionPolicy Bypass` for its own invocation, so it works without changing any machine-wide setting:

```powershell
.\tools\logs-collector\COLLECT-LOGS.cmd
```

```bash
./tools/logs-collector/collect-logs.sh
```

With no options it asks which test to collect. Double-clicking it in Explorer works and asks the same question.

To run the `.ps1` directly anyway, bypass the policy for that one command — this changes nothing permanently:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\logs-collector\Collect-Logs.ps1
```

| Flag | Effect |
|---|---|
| `-Test` / `--test` | Shortcut for a known blueprint, by number or name: `1` system-test · `2` ivi-isolated-test · `3` ada-isolated-test · `4` v2x-isolated-test · `5` netcheck-test |
| `-Blueprint` / `--blueprint` | Collect any deployed blueprint by name, listed or not |
| `-BaseUrl` / `--base-url` | CarSky gateway; default `https://hackathon-2.carsky.io` |
| `-ApiKeyFile` / `--api-key-file` | File holding the CarSky REST key; default `secrets/carsky-api-key.txt` |
| `-Port` / `--port` | Local port the ADB tunnel is expected on, or opened on if nothing is serving it yet; default 5555 |
| `-Token` / `--token` | ADB tunnel token to use instead of `secrets/reach-adb-token-ivi.txt`, when this run has to open its own tunnel |
| `-KeepTunnel` / `--keep-tunnel` | Leave a tunnel this run opened running after the run finishes; has no effect on a tunnel found already serving `-Port` |
| `-Tail` / `--tail` | Lines pulled per node log; default 50000 -- deep enough that a node's periodic pcap-rotation export reliably falls inside the window |
| `-OutRoot` / `--out-root` | Where the run folder is created; default `.\test-report` |

Exit 0 collected, 1 the deployment is not running or another hard stop, 2 a usage error.

The shortcuts are conveniences, not a whitelist — the node list of whatever blueprint you name decides what is collected:

```powershell
.\tools\logs-collector\COLLECT-LOGS.cmd -Test 2
.\tools\logs-collector\COLLECT-LOGS.cmd -Blueprint phase0_smoked_test -Tail 20000
```

## 2 · The ADB half opens its own tunnel

If nothing is already serving `-Port`, the collector opens its own tunnel from `secrets\reach-adb-token-ivi.txt` — the same token `INSTALL-IVI-APK.cmd` reads — and closes it again when the run finishes unless `-KeepTunnel` is passed. A tunnel another tool already has serving `-Port` is reused as-is and never touched.

Only when no tunnel can be opened at all — no token file, no `reach-backend.exe` under `tools\apk-uploader\reach_be\`, or a stale token the gateway rejects — are the guest-side files skipped, reported rather than failed, because a node-side collection is useful on its own. The run's `WARN` lines say which of those it was; the fix is usually re-copying the token, same as `INSTALL-IVI-APK.cmd`'s own token-refresh flow. Opening the tunnel by hand still works as a fallback:

```powershell
.\tools\apk-uploader\INSTALL-IVI-APK.cmd -SkipInstall -KeepTunnel
```

## 3 · What a run produces

A run ends in a checklist, and `summary.txt` is that checklist in plain text so it can be read after the window is gone.

| File | What it proves |
|---|---|
| `node-<slug>.txt` | One per container node, from the REST log endpoint |
| `skycraft-<slug>-vmhost.txt` | The Skycraft VM host's own output — WebRTC/GPU, nothing the app wrote |
| `app-logcat.txt` | The IVI app's tagged logcat — the `[RX]` evidence |
| `app-crash.txt` | Android's separate crash buffer |
| `guest-udp6.txt` | `/proc/net/udp6`, proving the socket is bound |
| `guest-ifaces.txt` | `ip -4 addr`, proving the guest took the pin address |
| `summary.txt` | This run's checklist |

`node-*.txt` from `m1-v2x-ecu` or `m1-ada-ecu` carries base64 pcap blocks — [pcap-extract](../pcap-extract/) turns those into `.pcap` files.

## Folder

`test-report/` holds the collected run folders and is git-ignored. The API key is read from `secrets/` at the repo root, also git-ignored, and is never printed, never written into an output file, and never put in a URL.
