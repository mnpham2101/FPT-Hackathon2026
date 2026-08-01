// Unit tests for VanetzaCpmCodec — the sole ICpmCodec implementation (subtask 1.0.2.3).
//
// Scope: CpmContent ⇄ ASN.1 UPER ⇄ CpmContent through vanetza::asn1::r2::Cpm, plus the
// F9 encode gate and the never-throw/never-crash decode contract. Exact byte sizes are
// deliberately NOT asserted here — the golden vectors in subtask 1.0.2.4 lock those.
//
// The nominal content is the frozen `nominal` golden vector from
// contracts/r1-cpm-profile.md §4, built in code exactly as in
// tests/codec/test_cpm_content_roundtrip.cpp.

#include "codec/vanetza_cpm_codec.hpp"

#include <cstdint>
#include <stdexcept>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace {

using v2x::codec::CpmContent;
using v2x::codec::DecodeError;
using v2x::codec::VanetzaCpmCodec;

// The frozen `nominal` golden vector, as a struct.
CpmContent NominalContent() {
  CpmContent c;
  c.stationId = 1201;
  c.referenceTime = 716084805123;
  c.referencePosition.latitude = 210285110;
  c.referencePosition.longitude = 1058048170;
  c.orientationAngle = 900;
  c.object.objectId = 7;
  c.object.measurementDeltaTime = -50;
  c.object.position.x = 2500;
  c.object.position.y = 120;
  c.object.position.xConfidence = 90;
  c.object.position.yConfidence = 90;
  c.object.velocity.x = 1520;
  c.object.velocity.y = 0;
  c.object.classification = 5;
  c.object.classConfidence = 95;
  return c;
}

TEST(VanetzaCpmCodec, EncodeDecodeIdentity) {
  const VanetzaCpmCodec codec;
  const CpmContent content = NominalContent();

  const std::vector<std::uint8_t> encoded = codec.encode(content);
  const auto decoded = codec.decode(encoded.data(), encoded.size());

  ASSERT_TRUE(std::holds_alternative<CpmContent>(decoded))
      << std::get<DecodeError>(decoded).reason;
  EXPECT_TRUE(std::get<CpmContent>(decoded) == content);
}

TEST(VanetzaCpmCodec, EncodeProducesNonEmptyUper) {
  // Size is observed, never asserted: the profiled CPM is a few dozen bytes, and the
  // exact figure is locked by the golden vectors (subtask 1.0.2.4), not here.
  const VanetzaCpmCodec codec;

  const std::vector<std::uint8_t> encoded = codec.encode(NominalContent());

  EXPECT_FALSE(encoded.empty());
}

TEST(VanetzaCpmCodec, RejectsMeasurementDeltaTimeAboveProfileBound) {
  // F9: the profile bound is +/-2047 ms. -2048 is wire-encodable as
  // DeltaTimeMilliSecondSigned but profile-invalid, so it must throw as well.
  const VanetzaCpmCodec codec;

  CpmContent above = NominalContent();
  above.object.measurementDeltaTime = 2048;
  EXPECT_THROW(codec.encode(above), std::out_of_range);

  CpmContent below = NominalContent();
  below.object.measurementDeltaTime = -2048;
  EXPECT_THROW(codec.encode(below), std::out_of_range);
}

TEST(VanetzaCpmCodec, AcceptsMeasurementDeltaTimeAtProfileBounds) {
  const VanetzaCpmCodec codec;

  for (const std::int16_t bound : {static_cast<std::int16_t>(2047),
                                   static_cast<std::int16_t>(-2047)}) {
    CpmContent content = NominalContent();
    content.object.measurementDeltaTime = bound;

    const std::vector<std::uint8_t> encoded = codec.encode(content);
    const auto decoded = codec.decode(encoded.data(), encoded.size());

    ASSERT_TRUE(std::holds_alternative<CpmContent>(decoded))
        << "measurementDeltaTime " << bound << ": "
        << std::get<DecodeError>(decoded).reason;
    EXPECT_TRUE(std::get<CpmContent>(decoded) == content);
  }
}

TEST(VanetzaCpmCodec, DecodeRejectsGarbage) {
  // Far too short to carry a profiled CPM, so UPER decoding must fail rather than
  // crash, throw or yield a half-filled CpmContent.
  const VanetzaCpmCodec codec;
  const std::vector<std::uint8_t> garbage{0xDE, 0xAD, 0xBE, 0xEF, 0x00};

  const auto decoded = codec.decode(garbage.data(), garbage.size());

  ASSERT_TRUE(std::holds_alternative<DecodeError>(decoded));
  EXPECT_FALSE(std::get<DecodeError>(decoded).reason.empty());
}

TEST(VanetzaCpmCodec, DecodeRejectsEmptyBuffer) {
  const VanetzaCpmCodec codec;
  const std::uint8_t sentinel = 0;

  const auto decoded = codec.decode(&sentinel, 0);

  ASSERT_TRUE(std::holds_alternative<DecodeError>(decoded));
  EXPECT_FALSE(std::get<DecodeError>(decoded).reason.empty());
}

}  // namespace
