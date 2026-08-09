# pcap-extract

Turns the base64 blocks inside a saved node log into `.pcap` files Wireshark can open. A capturing node emits its rotated pcap to stdout between `[PCAP-BEGIN <name>]` and `[PCAP-END]` markers, because the log is that node's only egress.

Usage only. Where the logs come from and what the capture proves are in [testing-guide.md](../../documents/Delivery/testing-guide.md).

## Tools

| Tool | OS | What it is |
|---|---|---|
| `EXTRACT-PCAP.cmd` | Windows | Double-clickable wrapper around `Extract-Pcap.ps1` |
| `Extract-Pcap.ps1` | Windows PowerShell 5.1 | The extractor; needs nothing but PowerShell |
| `extract_pcap.sh` | Linux, macOS, Git Bash | The same extractor |

The block format is frozen by the producers, `V2X_ECU/capture.sh` and `ADA_ECU/capture.sh` — change it there, then in every reader.

## 1 · Run it

**On Windows, run the `.cmd`, not the `.ps1`.** A fresh Windows blocks `.ps1` files from running — `.\Extract-Pcap.ps1` fails with *"running scripts is disabled on this system"* (`UnauthorizedAccess`). The `.cmd` wrapper carries `-ExecutionPolicy Bypass` for its own invocation, so it works without changing any machine-wide setting:

```powershell
.\tools\pcap-extract\EXTRACT-PCAP.cmd .\test-report\system-test
```

```bash
./tools/pcap-extract/extract_pcap.sh ./test-report/system-test
```

The path is required. Double-clicking the `.cmd` in Explorer works, but with nothing to work on PowerShell will ask for the path — pass it as an argument instead.

To run the `.ps1` directly anyway, bypass the policy for that one command — this changes nothing permanently:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\pcap-extract\Extract-Pcap.ps1 .\test-report\system-test
```

| Argument | Effect |
|---|---|
| `<log-file>` | One saved node log |
| `<log-dir>` | A directory of them: every `node-*.txt` in it is processed — the layout [logs-collector](../logs-collector/) writes |
| `-OutDir` / `-o` | Write the `.pcap` files here instead of beside the input; must exist |

Exit 0 every block extracted, 1 no block found at all — which usually means the node is missing `NET_RAW` — 2 a usage error, 3 at least one block failed and the reason was printed.

## 2 · What it will not do

Each of these is reported, never silent.

- **A truncated final block is not written.** Half a file must not masquerade as a complete capture.
- **No silent clobber.** An existing target gets `-2`, `-3`, … before `.pcap`; after `-99` the block fails rather than overwriting.
- **The block name is untrusted.** Any path component is stripped, so `[PCAP-BEGIN ../../evil.pcap]` writes `evil.pcap` inside the output directory and cannot escape it.
- **One failing block never hides the healthy ones.**

## 3 · Only two images carry a pcap

`m1-v2x-ecu` and `m1-ada-ecu`. The other images print `[CAP]` lines or nothing, so a log from them has no block to extract and exit 1 is the correct answer rather than a fault.
