# Telux Parity Notes & Port Plan (R7)

> The R7 doc deliverable of [v2x-ecu-hld.md](v2x-ecu-hld.md) §4, and the written half of the R7 acceptance §12 tracks — it closes that row together with the CI import gate ([tools/check_transport_imports.py](../tools/check_transport_imports.py)): why the seam's names and call order mirror the telux radio surface, and exactly what changes when the node runs against real modem hardware. Seam text: [src/adapter/i_radio_adapter.hpp](../src/adapter/i_radio_adapter.hpp) (frozen, referenced not restated). Requirement authority: [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) §2 R7, §3(b), §4.

## 1. Seam surface as frozen

The seam mirrors the RADIO surface only (HLD D1): the ADA-facing R2 link is deliberately outside it and stays UDP on hardware.

| Frozen method | M1 `StubRadioAdapter` | Telux-backed implementation |
|---|---|---|
| `RadioResult init()` | straight delegation to `ModemStub::init()`; result returned verbatim | radio-manager instantiation + wait for the SDK's ready/service-available state *(telux symbols unconfirmed — §5)* |
| `RadioResult configure(const RadioConfig& config)` | delegation to `ModemStub::configure()`; the stub stores the `RadioConfig` | RF/PC5 configuration: service-ID and flow parameters for the receive path *(unconfirmed)* |
| `RadioResult subscribeRx(RxCallback callback)` | asks the stub first, then binds `net::UdpSocket` to `RadioConfig::rx_port` with `kRxPollTimeout` armed and starts one Rx thread; bind failure → `SubscribeFailed` | create the Rx subscription for the configured service ID and pump the SDK-delivered payloads into `callback` *(capability confirmed in principle by the stale product-fit note, symbols unconfirmed)* |
| `RadioResult send(const std::vector<std::uint8_t>& bytes)` | declared per R7, no M1 caller, logs one line and returns `NotSupported` — **R10 deferred** (report §4 decision, 2026-07-30) | create a Tx flow and write the UPER octets to it *(unconfirmed; arrives with R10, not with the port)* |

- Payload boundary is identical on both sides: the callback receives the raw above-MAC octets, so nothing above the seam sees the difference (stale-note evidence: §5).
- `RxCallback` and `RadioConfig` are the seam's only data types; a telux implementation reuses `RxCallback` unchanged and re-freezes `RadioConfig` (§4, bullet 4).

## 2. Call-order constraints

- Legal order is `init` → `configure` → `subscribeRx` (→ `send`, R10-deferred); each call is legal from exactly one state of the R8 FSM `Idle → Initialized → Configured → RxSubscribed` ([src/stub/modem_stub.hpp](../src/stub/modem_stub.hpp)).
- Out-of-order calls are rejected by the `ModemStub` FSM — not by the adapter: state unchanged, observer notified with `EventKind::Reject`, and the call returns `InitFailed` / `ConfigureRejected` / `SubscribeFailed` respectively.
- The order is not a stub artifact: it is a radio manager's lifecycle — a radio object must exist before its RF/service parameters can be set, and parameters must be set before a receive subscription can name what to receive. A telux-backed implementation inherits the same precondition chain from the SDK, so the seam's order stays correct after the port.
- `configure` is the only call carrying data forward: `subscribeRx` consumes what `configure` stored (`stub.config().rx_port` in M1), which is why skipping `configure` cannot be recovered by `subscribeRx`.

## 3. Result-code mapping

`RadioResult` is the seam's whole error vocabulary — seam calls never throw, so every hardware failure must land in one of these five enumerators.

| `RadioResult` | M1 meaning | Expected telux error class |
|---|---|---|
| `Ok` | call accepted; state advanced | SDK success status *(unconfirmed)* |
| `InitFailed` | `init` called out of order, or the `init_fail` plan exhausted its retries | radio manager unavailable / service not ready *(unconfirmed)* |
| `ConfigureRejected` | `configure` out of order, or the `configure_reject` plan exhausted its retries | invalid or unsupported RF/flow parameters *(unconfirmed)* |
| `SubscribeFailed` | stub rejection (out of order), or the Rx socket failed to bind | Rx subscription creation refused / no such service ID *(unconfirmed)* |
| `NotSupported` | `send` — R10 deferred, no transmit path exists | *(no telux analogue; the code exists for the deferral, not for a hardware failure)* |

- No new enumerator is needed for the port: hardware failures classify into the four existing failure codes by which lifecycle step failed. Adding one would be a re-freeze (§4, bullet 4).

## 4. Port plan

Work order for the porter:

1. **Add `TeluxRadioAdapter` implementing the frozen header unchanged**, and drop it in at the single construction site: [src/main.cpp](../src/main.cpp) step 5 (`v2x::adapter::StubRadioAdapter adapter(stub, ...)`). That construction line and its `#include` are the only edits outside the new files — `main` afterwards still calls `init` / `configure` / `subscribeRx` / `stop` through the seam, and every other consumer holds `IRadioAdapter`.
2. **`ModemStub` stays** as bench and fault-injection equipment, off the hardware path: it owns the FSM, the `FAULT_PLAN` plans and the D2 recovery table, which remain the way faults are exercised deterministically. The telux adapter does not delegate to it; selecting stub vs telux is a composition-root choice, and both keep the same seam contract.
3. **Threading delta.** M1's adapter owns an Rx thread and a poll-timeout shutdown (`kRxPollTimeout` on the Rx socket, `stop()` joins). A telux SDK is *expected* to deliver received payloads on its own callback/dispatcher thread *(expected, not verified — §5)*, so the telux adapter's job shrinks to marshalling SDK callbacks onto `RxCallback` plus unsubscribing in `stop()`; it owns no receive loop. [src/net/udp_socket.hpp](../src/net/udp_socket.hpp) then leaves the radio path entirely and remains in use only by the intra-ego forwarder.
4. **Config delta.** `RadioConfig::rx_port` is a UDP artifact of the stub — a real radio is addressed by service/interface identifiers, not a listen port. Extending or replacing that struct is a **re-freeze point**: per the contract-first principle, re-freezing means updating every consumer (the seam header, both adapters, `ModemStub::config()`, and `main`'s `RadioConfig{cfg.listen_port}` construction) in one change, and the env set of HLD §6 alongside it.
5. **Unchanged above the seam** — the node's focus goal, hardware portability: the R9 pipeline (`decode → validate → dedupe → build`), the Vanetza codec seam, the R1/R2 contract bindings, the ADA forwarder, and the R18 `[EVT]` event log. None of them names a transport type, and the CI import gate keeps that true permanently.

## 5. Evidence & limits

Verified in-repo:

- The frozen seam and its five result codes, the `StubRadioAdapter` behaviour described in §1–§3, and the `ModemStub` FSM/fault/recovery ownership — all read from the landed sources linked above.
- Transport-blindness above the seam: `check_transport_imports.py` exits 0 over the tree and runs as a CI gate.
- The Rx path end to end: the `v2x-comms-check` CI lane sends the golden vectors at the listen port and asserts the `rx_datagram → decode_ok → r2_forwarded` `[EVT]` chain ([tools/comms_check/check_v2x_log.py](../../tools/comms_check/check_v2x_log.py)).

Needs the telux SDK to confirm — treat every *(unconfirmed)* cell above as a placeholder, not an API:

- **No authoritative telux reference exists in this repo.** The telux symbol names, their exact signatures, and their error/status types are unverified here and must be confirmed against Qualcomm's Telematics SDK headers before the port. Nothing in §1/§3 should be read as a real API name.
- The one in-repo mention is the **stale** product-fit note ([m1-product-fit-quectel-modem.md](../../requirements/deprecated/requirement-analysis/m1-product-fit-quectel-modem.md), reference-only per CLAUDE.md): it records the `telux::cv2x` radio API exposing the C-V2X radio as rmnet IP/non-IP interfaces with an Rx-subscription and Tx-flow socket fd, control via QMI. That supports the *capability* mapping in §1 and the payload-boundary claim; the call names it cites are not re-asserted here.
- SDK threading and delivery semantics (bullet 3) are expected behaviour inferred from that socket-fd model, not verified.
- SDK access itself is vendor-licensed and out of M1 scope: telux porting is declined for M1 (report §4), which is why this document, not code, is the R7 deliverable.
