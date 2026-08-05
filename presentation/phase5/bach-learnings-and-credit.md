# Phase 5 IVI — Learnings (Bach) + credit note for Lead

Author: **Vũ Xuân Bách** · Git: `bachxvu15072007-sudo <bachxvu15072007@gmail.com>`  
Use: paste learnings into Minh’s homepage / shared deck; paste the credit block into Discord/chat.

## Learnings (homepage / slide-ready)

### Deploy IVI on CarSky

- Artifacts **AAOS** = VM image + host package — not the HMI APK.
- Install path: Room Running → ADB tunnel → `adb install -r app-debug.apk` → launch `com.hackathon.v2x.ivi/.MainActivity`.
- Tunnel: `reach-backend adb` on `localhost:5555`; auth key = node `a8k_…` token.

### R4 path and HMI

- UDP listen **47300** → `R4ListenerService` → kotlinx deserialize → ViewModels → `CanvasWarningView`.
- Wake-on-warning: Active → WarningView; Idle → restore previous mode (default Home).
- Ghost C only from `source=v2x_relayed` (R19); unknown `warningType` preserved on the wire.

### Debug lessons

- Guest without pin IP `10.99.0.13` → ADA eth TX never reaches IVI; loopback inject still works.
- Missing Guava on Hilt builds → `ImmutableMap` crash at launch.
- UTF-8 BOM in injected JSON → parse “Not a JSON object”.

Deck detail: [phase5-ivi-deck.md](phase5-ivi-deck.md) §08. Guide: [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md).

## Credit / author rewrite — message for Minh

### Numbers (git, this clone)

| Scope | Count | Notes |
| --- | --- | --- |
| All commits authored by Bach | **59** | `git log --all --author=bachxvu15072007-sudo` |
| Commits touching `IVI_ECU/` | **17** | shortlog on that path (includes rebase duplicates) |
| Unique IVI subjects | **9** | after dedupe by commit message |
| Tagged IVI work (`[4.…]` / `[16.…]` / `[17.…]`) | **8** unique subjects | wake-on-warning, Hilt stack, tests, deploy docs, HomeView/BOUND, etc. |

Preferred credit identity:

- Name: `bachxvu15072007-sudo` (or display **Vũ Xuân Bách** if Lead rewrites `author.name`)
- Email: `bachxvu15072007@gmail.com`

### Copy-paste chat

> Minh ơi, Phase 5 IVI — Vũ Xuân Bách (`bachxvu15072007-sudo <bachxvu15072007@gmail.com>`).
>
> Git đếm được: **59** commit toàn repo dưới author này; **17** chạm `IVI_ECU/` (**9** unique subject sau rebase). Việc chính: wake-on-warning, Hilt R4→UI, God View / HomeView + `BOUND :47300`, tests, deploy docs.
>
> Hôm qua anh override nhiều commit — nhờ anh đổi author các commit Phase 5 IVI về email trên để ghi nhận contributor. Cảm ơn anh!

Re-count anytime:

```powershell
git shortlog -sn --all -- IVI_ECU
git log --all --author="bachxvu15072007-sudo" --oneline -- IVI_ECU
```
