#ifndef V2X_ECU_CODEC_VANETZA_CPM_CODEC_HPP
#define V2X_ECU_CODEC_VANETZA_CPM_CODEC_HPP

// V2X_ECU/src/codec/vanetza_cpm_codec.hpp — the sole ICpmCodec implementation
// (subtask 1.0.2.3). It maps CpmContent ⇄ ASN.1 UPER octets of an ETSI TS 103 324
// v2.1.1 CollectivePerceptionMessage, through `vanetza::asn1::r2::Cpm` (F2: the
// release-2 wrapper only; the bare release-1 alias is banned under V2X_ECU/src/).
//
// Message shape is the frozen profile contracts/r1-cpm-profile.md §2: exactly two
// containers — [id=1] OriginatingVehicleContainer and [id=5] PerceivedObjectContainer
// — and exactly one perceived object. Mandatory ASN.1 fields the profile does not
// carry in CpmContent are written with their CDD "unavailable"/fixed named values
// and ignored on decode (profile §3 encoding note).
//
// Seam rule (HLD D3): this is a PURE REPRESENTATION TRANSFORM. No unit conversion,
// no hypot, no confidence scaling, no clamping — integers are copied across. F6/F7/F1
// all happen above the seam, in the R9 pipeline.
//
// This header deliberately names no Vanetza or asn1c type: the codec library stays an
// implementation detail of the .cpp, so seam consumers need no ASN.1 include path.
//
// Wire format (F5): raw CollectivePerceptionMessage, one PDU per UDP datagram — no
// GeoNetworking/BTP envelope is added or expected.

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "codec/cpm_codec.hpp"

namespace v2x::codec {

class VanetzaCpmCodec final : public ICpmCodec {
 public:
  // Encode a profiled CpmContent to raw UPER octets.
  //
  // Throws std::out_of_range when |object.measurementDeltaTime| > 2047 — the F9
  // profile bound. Note that the ASN.1 wire range of DeltaTimeMilliSecondSigned is
  // −2048..2047, so −2048 is wire-encodable yet profile-invalid and throws too.
  //
  // Throws std::runtime_error (raised by Vanetza's asn1c PER wrapper) when the
  // underlying ASN.1 encoding fails — e.g. a CpmContent field outside its CDD
  // range. Never returns a partially-encoded or empty buffer instead.
  std::vector<std::uint8_t> encode(const CpmContent& content) const override;

  // Decode raw UPER octets back into a CpmContent.
  //
  // Never throws and never crashes: malformed, truncated, empty or null input —
  // and any CPM that does not match the frozen profile shape — yields a
  // DecodeError carrying a short human-readable reason.
  std::variant<CpmContent, DecodeError> decode(const std::uint8_t* data,
                                               std::size_t len) const override;
};

}  // namespace v2x::codec

#endif  // V2X_ECU_CODEC_VANETZA_CPM_CODEC_HPP
