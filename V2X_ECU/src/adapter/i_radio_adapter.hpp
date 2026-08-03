#ifndef V2X_ECU_ADAPTER_I_RADIO_ADAPTER_HPP
#define V2X_ECU_ADAPTER_I_RADIO_ADAPTER_HPP

// V2X_ECU/src/adapter/i_radio_adapter.hpp — the FROZEN R7 radio-adapter seam
// (subtask 7.1.3.1, HLD decision D2). Freeze discipline: later subtasks may
// NOT alter this interface text without re-freezing across every consumer.
//
// Shape: init() · configure(RadioConfig) · subscribeRx(RxCallback) ·
// send(bytes), with typed result codes — names and call order mirror the
// telux radio surface, documented in doc/telux-parity-and-port-plan.md
// (7.1.3.6).
//
// Seam rule (HLD D1): this seam mirrors the RADIO (telux) surface only. The
// intra-ego ADA link (R2 forwarding) is not behind it, and everything above
// the seam stays transport-blind — no socket/transport includes here
// (tools/check_transport_imports.py enforces this permanently).
//
// R10 deferral (report §4, 2026-07-30): `send` is declared per R7 but nothing
// calls it in M1; StubRadioAdapter::send returns RadioResult::NotSupported.

#include <cstdint>
#include <functional>
#include <vector>

namespace v2x::adapter {

// Radio configuration passed at configure(). Deliberately minimal: the Rx
// port is all this receive-only node needs (fed from LISTEN_PORT, HLD §6);
// R10 tx-side fields arrive only with a future re-freeze.
struct RadioConfig {
  std::uint16_t rx_port;
};

// Invoked once per received datagram; payload = the raw UPER CPM octets.
using RxCallback = std::function<void(const std::vector<std::uint8_t>&)>;

// Typed result codes for every seam call.
enum class RadioResult {
  Ok,
  InitFailed,
  ConfigureRejected,
  SubscribeFailed,
  NotSupported,
};

// Log-friendly name of a result code (inline, keeps the seam header-only).
inline const char* toString(RadioResult result) {
  switch (result) {
    case RadioResult::Ok:
      return "Ok";
    case RadioResult::InitFailed:
      return "InitFailed";
    case RadioResult::ConfigureRejected:
      return "ConfigureRejected";
    case RadioResult::SubscribeFailed:
      return "SubscribeFailed";
    case RadioResult::NotSupported:
      return "NotSupported";
  }
  return "Unknown";  // unreachable for valid enumerators; silences -Wreturn-type
}

// The seam. Pure virtual and non-copyable (deleted copy ops per C++ Core
// Guidelines C.67); methods in the D2 call order:
// init → configure → subscribeRx (→ send, R10-deferred).
class IRadioAdapter {
 public:
  IRadioAdapter() = default;
  IRadioAdapter(const IRadioAdapter&) = delete;
  IRadioAdapter& operator=(const IRadioAdapter&) = delete;
  virtual ~IRadioAdapter() = default;

  virtual RadioResult init() = 0;
  virtual RadioResult configure(const RadioConfig& config) = 0;
  virtual RadioResult subscribeRx(RxCallback callback) = 0;
  // Declared per R7; R10 deferred — no M1 caller; stub returns NotSupported.
  virtual RadioResult send(const std::vector<std::uint8_t>& bytes) = 0;
};

}  // namespace v2x::adapter

#endif  // V2X_ECU_ADAPTER_I_RADIO_ADAPTER_HPP
