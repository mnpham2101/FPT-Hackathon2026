# Deployment screenshots

Illustrations for [phase5-ivi-deploy.md](../phase5-ivi-deploy.md). Save PNGs here under exactly these names — the document links them by filename.

| Filename | Step | What to capture |
|---|---|---|
| `ivi-adb-local-adb.png` | Step 2, sub-steps 3–5 | The connected device in the Devices panel, the **IVI ADB** widget selected in its Widgets list, the **IVI ADB** tab open in the panel below the Stage, and **Local ADB** at that panel's top right |

**Capture from a device that is actually connected** — a green dot beside the device name, and an ADB SHELL panel that reached a `trout_arm64:/ $` prompt. A capture taken before the guest finished booting shows an empty shell and teaches the reader to expect one.

**Redact the `a8k_` token.** It is derived per device and a redeploy mints a new one, but a screenshot of the Local ADB dialog with the token legible commits a live credential — capture the button, never the open dialog.

Pasting an image into a chat does not create a file here — save it from the browser or screenshot tool into this folder, then commit it.
