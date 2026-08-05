// Parse-reject corpus test (subtask 3.2.3.3): both parsers walk the
// structurally invalid corpus under tests/fixtures/malformed/ with zero
// crashes and correct counters. Every case asserts its EXPECTED disposition —
// Reject with an exact reason, or ToleratedAdditive (accepted) — with no
// either-outcome branch; then a whole-corpus run proves both parser instances
// still accept the valid frozen samples afterwards.
//
// Corpus provenance — each file derives from the frozen samples under
// tests/fixtures/samples/ with exactly one defect injected (plan Task Group
// 2.3, subtask 3.2.3.3 scope):
//   R2 side (r2-object.json base):
//     r2-empty-line.json           empty datagram body (zero-byte file)
//     r2-truncated.json            JSON cut mid-token
//     r2-wrong-type.json           "type": "cam" instead of the const "v2x_object"
//     r2-missing-object.json       required "object" key removed
//     r2-missing-distance.json     required "object.distance" key removed
//     r2-distance-non-numeric.json "object.distance" is a string
//     r2-unknown-extra-field.json  extra keys at both levels — MUST be
//                                  tolerated (R2 additive evolution)
//   R3 side (r3-tracked-object.json base, source flipped to own_sensor —
//   the frozen sample itself is relayed-shaped):
//     r3-empty-line.json           empty JSONL line (zero-byte file)
//     r3-truncated.json            JSON cut mid-token
//     r3-missing-speed.json        required "speed" key removed
//     r3-confidence-out-of-range.json  confidence 1.5 against the 0..1 bound
//     r3-source-v2x-relayed.json   source "v2x_relayed" — a detector cannot
//                                  mint relayed entries (D6)
//     r3-unknown-extra-field.json  extra key — tolerated (additive evolution)

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "parser/r2_parser.hpp"
#include "parser/r3_parser.hpp"

namespace {

using ada::parser::R2Parser;
using ada::parser::R2RejectReason;
using ada::parser::R3Parser;
using ada::parser::R3RejectReason;

enum class Side { R2, R3 };
enum class Disposition { Reject, ToleratedAdditive };

struct CorpusCase {
  const char* file;
  Side side;
  Disposition disposition;
  // Exactly one of the two is meaningful, per side, and only for Reject.
  R2RejectReason r2Reason;
  R3RejectReason r3Reason;
};

constexpr CorpusCase kCorpus[] = {
    // R2 side
    {"r2-empty-line.json", Side::R2, Disposition::Reject, R2RejectReason::EmptyInput,
     R3RejectReason::None},
    {"r2-truncated.json", Side::R2, Disposition::Reject, R2RejectReason::MalformedJson,
     R3RejectReason::None},
    {"r2-wrong-type.json", Side::R2, Disposition::Reject, R2RejectReason::WrongType,
     R3RejectReason::None},
    {"r2-missing-object.json", Side::R2, Disposition::Reject, R2RejectReason::MissingField,
     R3RejectReason::None},
    {"r2-missing-distance.json", Side::R2, Disposition::Reject, R2RejectReason::MissingField,
     R3RejectReason::None},
    {"r2-distance-non-numeric.json", Side::R2, Disposition::Reject,
     R2RejectReason::WrongFieldType, R3RejectReason::None},
    {"r2-unknown-extra-field.json", Side::R2, Disposition::ToleratedAdditive,
     R2RejectReason::None, R3RejectReason::None},
    // R3 side
    {"r3-empty-line.json", Side::R3, Disposition::Reject, R2RejectReason::None,
     R3RejectReason::EmptyInput},
    {"r3-truncated.json", Side::R3, Disposition::Reject, R2RejectReason::None,
     R3RejectReason::MalformedJson},
    {"r3-missing-speed.json", Side::R3, Disposition::Reject, R2RejectReason::None,
     R3RejectReason::MissingField},
    {"r3-confidence-out-of-range.json", Side::R3, Disposition::Reject, R2RejectReason::None,
     R3RejectReason::OutOfRangeValue},
    {"r3-source-v2x-relayed.json", Side::R3, Disposition::Reject, R2RejectReason::None,
     R3RejectReason::NonOwnSensorSource},
    {"r3-unknown-extra-field.json", Side::R3, Disposition::ToleratedAdditive,
     R2RejectReason::None, R3RejectReason::None},
};

// Whole file as raw bytes — the corpus deliberately contains non-JSON.
std::string LoadCorpusFile(const std::string& name) {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/malformed/" + name;
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.is_open()) << "cannot open corpus file: " << path;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

nlohmann::json LoadSample(const std::string& name) {
  const std::string path = std::string(ADA_FIXTURE_DIR) + "/samples/" + name;
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open sample: " << path;
  return nlohmann::json::parse(in);
}

class ParseRejectCorpusTest : public testing::TestWithParam<CorpusCase> {};

TEST_P(ParseRejectCorpusTest, CaseHasExactlyItsExpectedDisposition) {
  const CorpusCase& c = GetParam();
  const std::string body = LoadCorpusFile(c.file);

  if (c.side == Side::R2) {
    R2Parser parser;
    const auto result = parser.parse(body);
    if (c.disposition == Disposition::ToleratedAdditive) {
      ASSERT_TRUE(result.accepted()) << c.file << " must be tolerated (additive evolution)";
      EXPECT_EQ(result.reason, R2RejectReason::None);
      EXPECT_EQ(parser.acceptedCount(), 1u);
      EXPECT_EQ(parser.rejectedCount(), 0u);
    } else {
      ASSERT_FALSE(result.accepted()) << c.file << " must be rejected";
      EXPECT_EQ(result.reason, c.r2Reason)
          << c.file << ": expected " << to_string(c.r2Reason) << ", got "
          << to_string(result.reason);
      EXPECT_EQ(parser.rejectedCount(c.r2Reason), 1u);
      EXPECT_EQ(parser.acceptedCount(), 0u);
    }
  } else {
    R3Parser parser;
    const auto result = parser.parse(body);
    if (c.disposition == Disposition::ToleratedAdditive) {
      ASSERT_TRUE(result.accepted()) << c.file << " must be tolerated (additive evolution)";
      EXPECT_EQ(result.reason, R3RejectReason::None);
      EXPECT_EQ(parser.acceptedCount(), 1u);
      EXPECT_EQ(parser.rejectedCount(), 0u);
    } else {
      ASSERT_FALSE(result.accepted()) << c.file << " must be rejected";
      EXPECT_EQ(result.reason, c.r3Reason)
          << c.file << ": expected " << to_string(c.r3Reason) << ", got "
          << to_string(result.reason);
      EXPECT_EQ(parser.rejectedCount(c.r3Reason), 1u);
      EXPECT_EQ(parser.acceptedCount(), 0u);
    }
  }
}

std::string CaseName(const testing::TestParamInfo<CorpusCase>& info) {
  std::string name = info.param.file;
  for (char& ch : name) {
    if (!std::isalnum(static_cast<unsigned char>(ch))) {
      ch = '_';
    }
  }
  return name;
}

INSTANTIATE_TEST_SUITE_P(Corpus, ParseRejectCorpusTest, testing::ValuesIn(kCorpus), CaseName);

// One parser instance per side survives the WHOLE corpus — zero crashes,
// counters exactly one tolerated accept and N−1 rejects per side — and still
// accepts the valid frozen sample afterwards.
TEST(ParseRejectCorpusWholeRun, ParsersSurviveAndStillAcceptTheValidSamples) {
  R2Parser r2;
  R3Parser r3;
  std::uint64_t r2Cases = 0;
  std::uint64_t r3Cases = 0;

  for (const CorpusCase& c : kCorpus) {
    const std::string body = LoadCorpusFile(c.file);
    if (c.side == Side::R2) {
      r2.parse(body);
      ++r2Cases;
    } else {
      r3.parse(body);
      ++r3Cases;
    }
  }

  // Exactly one tolerated-additive case per side; every other case rejected.
  EXPECT_EQ(r2.acceptedCount(), 1u);
  EXPECT_EQ(r2.rejectedCount(), r2Cases - 1);
  EXPECT_EQ(r3.acceptedCount(), 1u);
  EXPECT_EQ(r3.rejectedCount(), r3Cases - 1);

  // The valid R2 sample still parses on the same instance.
  const auto r2Result = r2.parse(LoadSample("r2-object.json").dump());
  EXPECT_TRUE(r2Result.accepted());
  EXPECT_EQ(r2.acceptedCount(), 2u);

  // The valid R3 sample, detector-shaped (the frozen sample is relayed-shaped,
  // which this parser rejects by design — D6), still parses likewise.
  nlohmann::json r3Doc = LoadSample("r3-tracked-object.json");
  r3Doc["source"] = "own_sensor";
  const auto r3Result = r3.parse(r3Doc.dump());
  EXPECT_TRUE(r3Result.accepted());
  EXPECT_EQ(r3.acceptedCount(), 2u);
}

}  // namespace
