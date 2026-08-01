# Walkthrough screenshots

Illustrations for [deploy-walkthrough-netcheck.md](../deploy-walkthrough-netcheck.md). Save PNGs here under exactly these names — the guide links them by filename.

| Filename | Step | What to capture |
|---|---|---|
| `nydus-inspector-blueprint.png` | M5 | Blueprint Inspector (click empty canvas): name/description, Locked, Deployments + New Deployment, ownership, Delete Blueprint |
| `nydus-inspector-v2x-ecu.png` | M7 | Node Inspector for V2X ECU: Image, Command, Capabilities (`NET_RAW` selected), Environment rows, Pins with address |
| `nydus-deploy-dialog.png` | M9 | Deploy Blueprint dialog: Deployment Name and the Device dropdown |

**Capture only from a verified-correct configuration** — see the walkthrough's "Mistakes already made" section. A screenshot of a broken config teaches the bug: the first M7 capture showed `NEXT_HOP_HOST = 10.99.0.2` and `ROLE = V2X`, both wrong.

Pasting an image into a chat does not create a file here — save it from the browser or screenshot tool into this folder, then commit it.
