#ifndef V2X_ECU_CODEC_CPM_CODEC_HPP
#define V2X_ECU_CODEC_CPM_CODEC_HPP

// V2X_ECU/src/codec/cpm_codec.hpp — the single R1 codec seam (report §3(a): one
// codec source behind one seam). Frozen shape: phase0-contract-freeze-hld.md §7.
//
// CpmContent is the profiled logical CPM in wire-native integer units; it ⇄ JSON
// via nlohmann and mirrors contracts/r1-cpm-content.schema.json 1:1 (synced copy
// at V2X_ECU/contracts/r1-cpm-content.schema.json; normative home is
// contracts/r1-cpm-profile.md §3–§4). Same JSON key names, same nesting.
//
// Seam rule (HLD D3): the codec is a PURE REPRESENTATION TRANSFORM,
// CpmContent ⇄ ASN.1 UPER octets. This header performs no unit conversion, no
// derivation, and no validation-by-clamping. R2's SI floats, F6 confidence
// conversion, F7 `object.distance = hypot(x, y)` and F1 sender-speed derivation
// all happen ABOVE the seam, in the R9 pipeline (Phase 1) — never here.
//
// Header-only: the JSON bindings are inline free functions, so there is no .cpp
// and no library target. The sole ICpmCodec implementation lands in subtask
// 1.0.2.3; this header stays free of any codec-library detail (F2).

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace v2x::codec {

// --- CpmContent — fully defined here, ahead of ICpmCodec, because
// std::variant<CpmContent, DecodeError> requires a complete type. ---

// Sender B's WGS84 reference position (schema: referencePosition).
struct CpmReferencePosition {
  std::int32_t latitude{};   // 1e-7 degree, -900000000..900000001; 900000001 = unavailable
  std::int32_t longitude{};  // 1e-7 degree, -1800000000..1800000001; 1800000001 = unavailable
};

inline bool operator==(const CpmReferencePosition& a, const CpmReferencePosition& b) {
  return a.latitude == b.latitude && a.longitude == b.longitude;
}

// Perceived object C's position in the sender-B frame (schema: object.position).
struct CpmObjectPosition {
  std::int32_t x{};             // CartesianCoordinateLarge, 0,01 m, -131072..131071
  std::int32_t y{};             // CartesianCoordinateLarge, 0,01 m, -131072..131071
  std::uint16_t xConfidence{};  // CoordinateConfidence, 0,01 m, 1..4096; 4096 = unavailable (F6)
  std::uint16_t yConfidence{};  // CoordinateConfidence, 0,01 m, 1..4096; 4096 = unavailable (F6)
};

inline bool operator==(const CpmObjectPosition& a, const CpmObjectPosition& b) {
  return a.x == b.x && a.y == b.y && a.xConfidence == b.xConfidence &&
         a.yConfidence == b.yConfidence;
}

// Perceived object C's velocity in the sender-B frame (schema: object.velocity).
struct CpmObjectVelocity {
  std::int16_t x{};  // VelocityComponentValue, 0,01 m/s, -16383..16383
  std::int16_t y{};  // VelocityComponentValue, 0,01 m/s, -16383..16383
};

inline bool operator==(const CpmObjectVelocity& a, const CpmObjectVelocity& b) {
  return a.x == b.x && a.y == b.y;
}

// The single perceived object carried by a profiled CPM (schema: object).
struct CpmPerceivedObject {
  std::uint16_t objectId{};           // Identifier2B, 0..65535 — B-assigned ID of C
  std::int16_t measurementDeltaTime{};  // DeltaTimeMilliSecondSigned, ms; profile-valid ±2047 (F9)
  CpmObjectPosition position;
  CpmObjectVelocity velocity;
  std::uint8_t classification{};   // TrafficParticipantType, 0..255; M1 uses passengerCar(5)
  std::uint8_t classConfidence{};  // ConfidenceLevel, 1..101; 101 = unavailable (F6)
};

inline bool operator==(const CpmPerceivedObject& a, const CpmPerceivedObject& b) {
  return a.objectId == b.objectId && a.measurementDeltaTime == b.measurementDeltaTime &&
         a.position == b.position && a.velocity == b.velocity &&
         a.classification == b.classification && a.classConfidence == b.classConfidence;
}

// Profiled logical CPM, wire-native integer units.
struct CpmContent {
  std::uint32_t stationId{};     // ItsPduHeader stationId of sender B, 0..4294967295
  std::uint64_t referenceTime{};  // TimestampIts, ms since 2004-01-01T00:00:00.000 TAI
  CpmReferencePosition referencePosition;
  std::uint16_t orientationAngle{};  // Wgs84AngleValue, 0,1 degree, 0..3601; 3601 = unavailable
  CpmPerceivedObject object;
};

inline bool operator==(const CpmContent& a, const CpmContent& b) {
  return a.stationId == b.stationId && a.referenceTime == b.referenceTime &&
         a.referencePosition == b.referencePosition &&
         a.orientationAngle == b.orientationAngle && a.object == b.object;
}

// --- JSON binding (inline, header-only) ---
//
// from_json uses j.at(...), which throws on a missing schema-required key; every
// CpmContent field is required, so there are no nullable fields and no
// std::optional here. Unknown extra keys are deliberately ignored (additive
// evolution). `classification` and `classConfidence` are std::uint8_t: nlohmann
// maps unsigned integral types to JSON numbers, so they serialize as plain
// numbers (5, 95), never as characters.

inline void to_json(nlohmann::json& j, const CpmReferencePosition& p) {
  j = nlohmann::json{{"latitude", p.latitude}, {"longitude", p.longitude}};
}

inline void from_json(const nlohmann::json& j, CpmReferencePosition& p) {
  j.at("latitude").get_to(p.latitude);
  j.at("longitude").get_to(p.longitude);
}

inline void to_json(nlohmann::json& j, const CpmObjectPosition& p) {
  j = nlohmann::json{{"x", p.x},
                     {"y", p.y},
                     {"xConfidence", p.xConfidence},
                     {"yConfidence", p.yConfidence}};
}

inline void from_json(const nlohmann::json& j, CpmObjectPosition& p) {
  j.at("x").get_to(p.x);
  j.at("y").get_to(p.y);
  j.at("xConfidence").get_to(p.xConfidence);
  j.at("yConfidence").get_to(p.yConfidence);
}

inline void to_json(nlohmann::json& j, const CpmObjectVelocity& v) {
  j = nlohmann::json{{"x", v.x}, {"y", v.y}};
}

inline void from_json(const nlohmann::json& j, CpmObjectVelocity& v) {
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
}

inline void to_json(nlohmann::json& j, const CpmPerceivedObject& o) {
  j = nlohmann::json{{"objectId", o.objectId},
                     {"measurementDeltaTime", o.measurementDeltaTime},
                     {"position", o.position},
                     {"velocity", o.velocity},
                     {"classification", o.classification},
                     {"classConfidence", o.classConfidence}};
}

inline void from_json(const nlohmann::json& j, CpmPerceivedObject& o) {
  j.at("objectId").get_to(o.objectId);
  j.at("measurementDeltaTime").get_to(o.measurementDeltaTime);
  j.at("position").get_to(o.position);
  j.at("velocity").get_to(o.velocity);
  j.at("classification").get_to(o.classification);
  j.at("classConfidence").get_to(o.classConfidence);
}

inline void to_json(nlohmann::json& j, const CpmContent& c) {
  j = nlohmann::json{{"stationId", c.stationId},
                     {"referenceTime", c.referenceTime},
                     {"referencePosition", c.referencePosition},
                     {"orientationAngle", c.orientationAngle},
                     {"object", c.object}};
}

inline void from_json(const nlohmann::json& j, CpmContent& c) {
  j.at("stationId").get_to(c.stationId);
  j.at("referenceTime").get_to(c.referenceTime);
  j.at("referencePosition").get_to(c.referencePosition);
  j.at("orientationAngle").get_to(c.orientationAngle);
  j.at("object").get_to(c.object);
}

// --- The seam ---

struct DecodeError { std::string reason; };

class ICpmCodec {
public:
    virtual ~ICpmCodec() = default;
    virtual std::vector<std::uint8_t> encode(const CpmContent&) const = 0;                  // → ASN.1 UPER; throws on un-encodable content (F9 bounds)
    virtual std::variant<CpmContent, DecodeError> decode(const std::uint8_t* data, std::size_t len) const = 0;
};

}  // namespace v2x::codec

#endif  // V2X_ECU_CODEC_CPM_CODEC_HPP
