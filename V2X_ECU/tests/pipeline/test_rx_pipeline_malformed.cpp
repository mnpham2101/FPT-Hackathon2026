// R9 malformed-input corpus: the acceptance test for "malformed or unknown
// input is rejected and counted, never crashes the node" (report §2 R9; Phase 1
// HLD §3 D3 stage 1).
//
// Unlike tests/pipeline/test_rx_pipeline.cpp — which fakes the codec to test the
// COMPOSITION — this suite wires the REAL v2x::codec::VanetzaCpmCodec behind the
// real Validator / Deduper / R2Builder / EventLog. That is the entire point:
// only the actual ASN.1 UPER decoder can prove that garbage octets produce a
// DecodeError instead of a segfault, an infinite loop or a throw. Reaching the
// assertions below IS the zero-crash evidence; every case additionally asserts
// that the reject detail is NOT the "unexpected_exception: " attribution
// rx_pipeline.cpp uses for an escaping throw — i.e. the codec rejected by
// contract, not merely caught by the pipeline's catch(...) safety net.
//
// ---------------------------------------------------------------------------
// CORPUS PROVENANCE (tests/fixtures/malformed/, local fixtures — NOT synced
// contracts, so they are absent from contracts/sync-manifest.json per HLD §4).
//
// Generation is deterministic and out-of-tree: the plan designates no in-repo
// path for a generator, so the recipe is recorded HERE and can be replayed
// byte-identically with CPython >= 3.9 (`random.Random(seed).randbytes(n)` is
// Mersenne-Twister, stable across versions; generated with 3.12.10). Source
// bytes for every derived case are the frozen golden vector
// tests/fixtures/golden/nominal.uper — N = 58 octets, first eight
// `02 0e 00 00 04 b1 02 9a`.
//
// ItsPduHeader wire layout, VERIFIED against those real bytes rather than
// assumed: offset 0 = protocolVersion (8 bits, value 2), offset 1 = messageId
// (8 bits, value 14 = cpm), offsets 2..5 = stationId (32 bits, 0x000004b1 =
// 1201 — station B, matching nominal.json). Both header integers therefore
// occupy exactly one octet each with a zero lower bound, so any octet value
// 0..255 is in range for them.
//
//   empty.uper                   0 B     — zero-length datagram.
//   truncated-nominal.uper      29 B     — nominal[0 : N/2] = first 29 of 58 octets.
//   random.uper                 58 B     — random.Random(20260801).randbytes(58);
//                                          first eight `97 c2 fa 8d bd 13 56 b4`.
//   wrong-message-id.uper       58 B     — nominal with offset 1: 0x0e -> 0x02
//                                          (cpm(14) -> cam(2)).
//   wrong-protocol-version.uper 58 B     — nominal with offset 0: 0x02 -> 0x03
//                                          (release 2 -> a non-existent release 3).
//   r1-variant.uper             58 B     — nominal with offset 0: 0x02 -> 0x01,
//                                          the release-1 protocolVersion.
//   oversized.uper            4096 B     — random.Random(20260802).randbytes(4096);
//                                          first eight `7a a3 9b ca 21 35 af 39`.
//                                          4 KiB is far beyond any legal profiled
//                                          CPM (58 B), so this case exercises the
//                                          size path as well as the content path.
//
// Two construction choices need stating, because the obvious alternatives do
// NOT reject and would have made silent pass-through look like success:
//
//  (1) oversized.uper is fixed-seed random, NOT "nominal + 0x00 padding to
//      4 KiB". Vanetza decodes through `uper_decode_complete`
//      (vanetza/asn1/asn1c_wrapper.cpp -> vanetza/asn1/support/uper_decoder.c),
//      which does not check for unconsumed trailing octets: a valid CPM
//      followed by any amount of padding decodes successfully. A padded golden
//      vector would therefore be forwarded, not rejected.
//
//  (2) r1-variant.uper edits only protocolVersion. The release-1 and release-2
//      ItsPduHeader have the identical wire shape (8 + 8 + 32 bits), so
//      protocolVersion is the only header-level release marker there is; the
//      release-1 CpmPayload *structure* differs, but this tree cannot produce
//      release-1 bytes (convention F2 bans the bare release-1 wrapper under
//      V2X_ECU/src/, and no release-1 encoder exists here). Honest label: this
//      fixture is a release-1-labelled header carrying release-2 payload bytes.
//
// ---------------------------------------------------------------------------
// EXPECTED OUTCOMES — 4 rejected, 3 tolerated BY FROZEN CONTRACT.
//
// The three header-edit cases are NOT rejectable by this pipeline, and asserting
// otherwise would be asserting a bug. contracts/r1-cpm-profile.md §3 freezes
// `header.protocolVersion=2` / `header.messageId=cpm(14)` as "encoded with their
// fixed values and IGNORED ON DECODE", and vanetza_cpm_codec.cpp implements
// exactly that (see its decode() comment). Both fields are in ASN.1 range for
// every octet value (verified above), the remaining bitstream is untouched, and
// CpmContent carries no header field for the stage-2 Validator to inspect.
// A header-only edit is therefore a well-formed profiled CPM with an advisory
// label the contract discards — decode_ok, then forwarded.
//
// So these three are asserted as what they are: PROFILE-TOLERATED negative
// controls that decode to the very same content as the golden nominal vector,
// which is itself the §3 ignore-on-decode rule under test — and which proves
// this suite is not passing trivially (a pipeline that rejected *everything*
// would fail here). Closing this gap means adding a header-conformance check to
// the R1 codec/profile; that re-freezes a Phase 0 contract and is out of scope
// for this subtask.

#include "pipeline/rx_pipeline.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "codec/vanetza_cpm_codec.hpp"
#include "contracts/r2_message.hpp"
#include "log/event_log.hpp"
#include "pipeline/deduper.hpp"
#include "pipeline/r2_builder.hpp"
#include "pipeline/validator.hpp"

namespace {

using std::chrono::milliseconds;
using v2x::codec::VanetzaCpmCodec;
using v2x::contracts::R2Message;
using v2x::log::EventLog;
using v2x::pipeline::Deduper;
using v2x::pipeline::R2Builder;
using v2x::pipeline::RxPipeline;
using v2x::pipeline::Validator;

constexpr char kPrefix[] = "[EVT] ";
constexpr std::size_t kPrefixLen = sizeof(kPrefix) - 1;

// Attribution prefix rx_pipeline.cpp puts on a reject caused by an escaping
// exception — its ABSENCE is what distinguishes "rejected by contract" from
// "caught by the catch(...) net".
constexpr char kUnexpected[] = "unexpected_exception:";

// Fixed pipeline rx-time stamp (ms epoch) and test-local dedupe window; both
// injected, exactly as in tests/pipeline/test_rx_pipeline.cpp.
constexpr std::int64_t kRxTimeMs = 1754006400000;
constexpr std::int64_t kDedupeWindowMs = 1500;

enum class Outcome {
  kDecodeReject,        // stage 1 returns DecodeError: reject + count, no sink call
  kToleratedByProfile,  // well-formed CPM with an ignored-on-decode header edit
};

struct MalformedCase {
  const char* stem;     // file name without the .uper extension
  Outcome outcome;      // what the frozen contracts require of this input
  const char* why;      // surfaced in failure output
};

// Order is also the order the whole-corpus test feeds them in.
const MalformedCase kCorpus[] = {
    {"empty", Outcome::kDecodeReject, "zero-length datagram"},
    {"truncated-nominal", Outcome::kDecodeReject, "first 29 of the golden 58 octets"},
    {"random", Outcome::kDecodeReject, "fixed-seed random 58 octets"},
    {"wrong-message-id", Outcome::kToleratedByProfile,
     "messageId cam(2): ignored on decode per profile section 3"},
    {"wrong-protocol-version", Outcome::kToleratedByProfile,
     "protocolVersion 3: ignored on decode per profile section 3"},
    {"r1-variant", Outcome::kToleratedByProfile,
     "protocolVersion 1: ignored on decode per profile section 3"},
    {"oversized", Outcome::kDecodeReject, "4 KiB of fixed-seed random octets"},
};
constexpr std::size_t kCorpusSize = sizeof(kCorpus) / sizeof(kCorpus[0]);
constexpr std::size_t kRejectedCases = 4;   // empty, truncated, random, oversized
constexpr std::size_t kToleratedCases = 3;  // the three header edits

// Binary mode is mandatory — the corpus is arbitrary octets, and text-mode
// translation would silently rewrite them. An empty file is legitimate here
// (empty.uper), so emptiness is never treated as a load failure.
std::vector<std::uint8_t> ReadFixture(const std::string& relative_path) {
  const std::string path = std::string(V2X_FIXTURE_DIR) + "/" + relative_path;
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  const std::string raw((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
  return std::vector<std::uint8_t>(raw.begin(), raw.end());
}

std::vector<std::uint8_t> ReadMalformed(const std::string& stem) {
  return ReadFixture("malformed/" + stem + ".uper");
}

std::vector<std::string> Lines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

nlohmann::json ParseEvtLine(const std::string& line) {
  EXPECT_EQ(line.rfind(kPrefix, 0), 0u) << "line missing [EVT] prefix: " << line;
  return nlohmann::json::parse(line.substr(kPrefixLen));
}

// Member declaration order is the construction order: every collaborator is
// declared before pipeline_, so the references it stores stay valid.
class MalformedPipelineTest : public ::testing::Test {
 protected:
  std::vector<R2Message> forwarded_;
  std::ostringstream log_out_;
  VanetzaCpmCodec codec_;  // the REAL ASN.1 codec — the subject of this suite
  Validator validator_;
  std::int64_t dedupe_clock_ms_ = 0;
  Deduper deduper_{milliseconds(kDedupeWindowMs), [this] { return dedupe_clock_ms_; }};
  R2Builder builder_;
  EventLog log_{log_out_};
  RxPipeline pipeline_{codec_,
                       validator_,
                       deduper_,
                       builder_,
                       log_,
                       [this](const R2Message& r2) { forwarded_.push_back(r2); },
                       [] { return kRxTimeMs; }};

  std::vector<std::string> EvtLines() const { return Lines(log_out_.str()); }

  nlohmann::json LastCounters() const {
    const auto lines = EvtLines();
    EXPECT_FALSE(lines.empty()) << "no [EVT] line emitted";
    if (lines.empty()) {
      return nlohmann::json::object();
    }
    return ParseEvtLine(lines.back()).at("counters");
  }

  void ExpectCounters(std::uint64_t rx, std::uint64_t decode_ok,
                      std::uint64_t decode_reject, std::uint64_t validate_reject,
                      std::uint64_t dedupe_drop, std::uint64_t r2_forwarded) const {
    const nlohmann::json c = LastCounters();
    EXPECT_EQ(c.at("rx_datagram").get<std::uint64_t>(), rx);
    EXPECT_EQ(c.at("decode_ok").get<std::uint64_t>(), decode_ok);
    EXPECT_EQ(c.at("decode_reject").get<std::uint64_t>(), decode_reject);
    EXPECT_EQ(c.at("validate_reject").get<std::uint64_t>(), validate_reject);
    EXPECT_EQ(c.at("dedupe_drop").get<std::uint64_t>(), dedupe_drop);
    EXPECT_EQ(c.at("r2_forwarded").get<std::uint64_t>(), r2_forwarded);
  }

  // No datagram may ever be attributed to an escaping exception: the codec must
  // reject by returning DecodeError, not by throwing into the catch(...) net.
  void ExpectNoUnexpectedException() const {
    EXPECT_EQ(log_out_.str().find(kUnexpected), std::string::npos)
        << "a stage threw instead of rejecting:\n"
        << log_out_.str();
  }

  // The golden nominal.json values, as they land in R2 after the stage-4a
  // derivations (asserted identically in test_rx_pipeline.cpp).
  void ExpectNominalR2(const R2Message& r2) const {
    EXPECT_EQ(r2.schemaVersion, 1);
    EXPECT_EQ(r2.type, "v2x_object");
    EXPECT_EQ(r2.stationId, 1201u);
    EXPECT_EQ(r2.object.objectId, 7);
    EXPECT_EQ(r2.object.timeOfMeasurement, -50);
    EXPECT_EQ(r2.rxTime, kRxTimeMs);
    EXPECT_NEAR(r2.object.position.x, 25.0, 1e-9);
    EXPECT_NEAR(r2.object.position.y, 1.2, 1e-9);
    EXPECT_NEAR(r2.object.distance, std::hypot(25.0, 1.2), 1e-9);
  }
};

class MalformedCorpusTest : public MalformedPipelineTest,
                            public ::testing::WithParamInterface<MalformedCase> {};

// One datagram, one documented outcome, zero crashes. onDatagram is noexcept, so
// an escaping exception would std::terminate rather than propagate — surviving to
// the assertions is the proof; ExpectNoUnexpectedException adds that the codec
// did not throw internally either.
TEST_P(MalformedCorpusTest, IsHandledExactlyAsTheContractsRequire) {
  const MalformedCase& c = GetParam();
  SCOPED_TRACE(std::string(c.stem) + ".uper — " + c.why);

  const std::vector<std::uint8_t> bytes = ReadMalformed(c.stem);
  pipeline_.onDatagram(bytes);

  const auto lines = EvtLines();
  ASSERT_FALSE(lines.empty());
  const nlohmann::json first = ParseEvtLine(lines.front());
  EXPECT_EQ(first.at("event").get<std::string>(), "rx_datagram");
  EXPECT_EQ(first.at("bytes").get<std::size_t>(), bytes.size());
  ExpectNoUnexpectedException();

  if (c.outcome == Outcome::kDecodeReject) {
    // Sink never called, one decode_reject counted, nothing forwarded.
    EXPECT_TRUE(forwarded_.empty());
    ExpectCounters(/*rx=*/1, /*decode_ok=*/0, /*decode_reject=*/1, /*validate_reject=*/0,
                   /*dedupe_drop=*/0, /*r2_forwarded=*/0);
    // Two lines only: rx_datagram then decode_reject — the datagram never
    // reached stage 2, so no decode_ok appears in between.
    ASSERT_EQ(lines.size(), 2u);
    const nlohmann::json reject = ParseEvtLine(lines.back());
    EXPECT_EQ(reject.at("event").get<std::string>(), "decode_reject");
    // A human-readable reason is part of the R9 "rejected AND logged" acceptance.
    EXPECT_FALSE(reject.at("detail").get<std::string>().empty());
    return;
  }

  // kToleratedByProfile: the header octets are discarded per profile §3, so the
  // decoded content — and the R2 message built from it — is the golden nominal
  // one. See the EXPECTED OUTCOMES note at the top of this file.
  ASSERT_EQ(forwarded_.size(), 1u);
  ExpectNominalR2(forwarded_.front());
  ExpectCounters(/*rx=*/1, /*decode_ok=*/1, /*decode_reject=*/0, /*validate_reject=*/0,
                 /*dedupe_drop=*/0, /*r2_forwarded=*/1);
}

// gtest test-name suffixes must be alphanumeric/underscore, so the hyphenated
// case names are transliterated — same convention as the golden-vector suite.
std::string CaseName(const ::testing::TestParamInfo<MalformedCase>& info) {
  std::string name = info.param.stem;
  for (char& ch : name) {
    if (ch == '-') {
      ch = '_';
    }
  }
  return name;
}

INSTANTIATE_TEST_SUITE_P(MalformedCorpus, MalformedCorpusTest,
                         ::testing::ValuesIn(kCorpus), CaseName);

// The whole corpus through ONE pipeline instance, then the golden vector: the
// strongest zero-crash evidence there is, because it also shows the pipeline is
// not WEDGED by garbage — the real codec still decodes a legal CPM afterwards.
//
// The dedupe clock steps a full window between datagrams: the three tolerated
// header variants decode to identical content (same stationId/objectId/
// measurement time), so without the step they would collapse into dedupe_drops
// and hide the forwarding behavior under test.
TEST_F(MalformedPipelineTest, WholeCorpusIsSurvivedAndLeavesThePipelineUsable) {
  for (const MalformedCase& c : kCorpus) {
    SCOPED_TRACE(std::string("corpus case ") + c.stem);
    pipeline_.onDatagram(ReadMalformed(c.stem));
    dedupe_clock_ms_ += kDedupeWindowMs;
  }

  ASSERT_EQ(forwarded_.size(), kToleratedCases);
  ExpectCounters(/*rx=*/kCorpusSize, /*decode_ok=*/kToleratedCases,
                 /*decode_reject=*/kRejectedCases, /*validate_reject=*/0,
                 /*dedupe_drop=*/0, /*r2_forwarded=*/kToleratedCases);
  ExpectNoUnexpectedException();

  // Not wedged: the frozen golden vector still decodes and forwards through the
  // very same codec and stage instances that just swallowed 4 KiB of garbage.
  pipeline_.onDatagram(ReadFixture("golden/nominal.uper"));

  ASSERT_EQ(forwarded_.size(), kToleratedCases + 1);
  ExpectNominalR2(forwarded_.back());
  ExpectCounters(/*rx=*/kCorpusSize + 1, /*decode_ok=*/kToleratedCases + 1,
                 /*decode_reject=*/kRejectedCases, /*validate_reject=*/0,
                 /*dedupe_drop=*/0, /*r2_forwarded=*/kToleratedCases + 1);
  ExpectNoUnexpectedException();
}

}  // namespace
