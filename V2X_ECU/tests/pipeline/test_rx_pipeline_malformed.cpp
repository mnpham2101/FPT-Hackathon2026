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
// ===========================================================================
// TWO CASE CATEGORIES — the architecture owner's adjudication of 2026-08-01.
//
// The corpus holds two kinds of input, and conflating them is what previously
// made this suite look like it had a 4-reject/3-tolerated "gap":
//
//  * STRUCTURALLY MALFORMED — the input is not a decodable UPER
//    CollectivePerceptionMessage at all (no bits, too few bits, corrupted
//    length/presence/CHOICE structure, or bits that were never a CPM).
//    Disposition::kReject. THIS is the class R9's "malformed-input corpus is
//    fully rejected" speaks about.
//
//  * PROFILE-TOLERATED NEGATIVE CONTROL — the input is a well-formed profiled
//    CPM that differs from the golden vector only in octets the frozen contract
//    says to DISCARD. contracts/r1-cpm-profile.md §3 freezes
//    `header.protocolVersion=2` / `header.messageId=cpm(14)` as "encoded with
//    their fixed values and IGNORED ON DECODE", vanetza_cpm_codec.cpp::decode()
//    implements exactly that, and CpmContent carries no header field for the
//    stage-2 Validator to inspect. A header-only edit is therefore NOT a
//    malformed input — it is a valid-but-differently-labelled one, and decoding
//    + forwarding it is the profile §3 rule working as frozen.
//    Disposition::kToleratedControl. Asserting a rejection here would be
//    asserting a bug, and would need a profile §3 re-freeze to become true.
//
// These controls also keep the suite from passing trivially: a pipeline that
// rejected *everything* would fail them, and they pin that the discarded header
// octets change nothing — each decodes to the very same CpmContent as the golden
// nominal vector.
//
// Every case therefore carries an EXPECTED DISPOSITION and the test asserts THAT
// EXACT disposition. There is deliberately no "either outcome passes" branch —
// such an assertion proves nothing.
//
// ---------------------------------------------------------------------------
// PREDICTED, NOT LOCALLY MEASURED. The dispositions below are derived from the
// wire-layout analysis recorded in the provenance block (which octet carries
// which ASN.1 element) plus the pinned Vanetza decoder's behaviour — they were
// NOT observed on a local run: the authoring host has no C++ toolchain, and
// convention F3 bans an ASN.1 decoder in Python, so nothing here could classify
// them empirically. The `v2x-core-build` CI lane is the oracle. If a prediction
// is wrong the lane goes RED and the fix is to flip that one case's
// `Disposition` field (the counter arithmetic is derived from the table, so it
// follows automatically) — that relabel loop is the intended design, not a
// failure of it. The failure output is built to make the relabel a one-liner:
// see Diagnose(), which prints the case, the expected disposition, whether the
// datagram decoded/forwarded, which reject counter fired, and the reject detail.
//
// ===========================================================================
// CORPUS PROVENANCE (tests/fixtures/malformed/, local fixtures — NOT synced
// contracts, so they are absent from contracts/sync-manifest.json per HLD §4).
//
// Generation is deterministic and out-of-tree: the plan designates no in-repo
// path for a generator, so the recipe is recorded HERE and can be replayed
// byte-identically with CPython >= 3.9 (`random.Random(seed).randbytes(n)` is
// Mersenne-Twister, stable across versions; generated with 3.12.10). Source
// bytes for every derived case are the frozen golden vector
// tests/fixtures/golden/nominal.uper — N = 58 octets,
// sha256 3f0bf84f385faf45cd0eb7456fdf0da163d97c9fff2aac25dab178d20ddaa407:
//
//   02 0e 00 00 04 b1 02 9a e7 eb f8 0e 11 6c d9 b5
//   52 d2 d5 57 ff ff ff 08 ed dd 0f 90 03 03 84 fc
//   41 80 08 05 80 10 00 3b e7 20 9c 40 59 80 1e 01
//   66 8b df f9 ff ff e0 16 f0 00
//
// --- Verified UPER bit map of nominal.uper -------------------------------
//
// Bit offsets are absolute from bit 0 = MSB of octet 0. Every row marked
// "= value" was CONFIRMED by decoding those exact bits out of the real fixture
// and matching the value in contracts/golden-vectors/nominal.json — this map is
// measured, not assumed, and it is what makes the two derived offsets below
// defensible rather than arbitrary.
//
//   bits    0..7    octet 0    header.protocolVersion            = 2
//   bits    8..15   octet 1    header.messageId                  = 14 (cpm)
//   bits   16..47   octets 2-5 header.stationId                  = 1201
//   bits   48..51              CpmPayload/ManagementContainer preamble
//   bits   52..93              managementContainer.referenceTime = 716084805123
//   bits   94..124             referencePosition.latitude        = 210285110
//   bits  125..156             referencePosition.longitude       = 1058048170
//   bits  157..192             positionConfidenceEllipse (3 x 12 bits, orientation = 3601)
//   bits  193..216             altitude (20 + 4 bits, value = 800001, confidence = 15)
//   bits  217..219             cpmContainers SEQUENCE-OF length determinant = 2 containers
//   bits  220..235             WrappedCpmContainer[0] header (containerId=1 + CHOICE index
//                              + OriginatingVehicleContainer presence preamble)
//   bits  236..254             orientationAngle: value = 900, confidence = 127
//   bits  255..300  octets     WrappedCpmContainer[1] header (containerId=5 +
//                   31..37     CHOICE index) + PerceivedObjectContainer
//                              (numberOfPerceivedObjects + perceivedObjects length
//                              determinant) + PerceivedObject OPTIONAL-presence bitmap.
//                              *** Every element in this span is STRUCTURAL — an
//                              identifier, a CHOICE index, a length determinant or a
//                              presence bitmap. There is not one plain value INTEGER
//                              in it. ***
//   bits  301..316             perceivedObject.objectId              = 7
//   bits  317..328             measurementDeltaTime                  = -50
//   bit   329                  position zCoordinate-absent flag
//   bits  330..347             position.xCoordinate.value            = 2500
//   bits  348..359             position.xCoordinate.confidence       = 90
//   bits  360..377             position.yCoordinate.value            = 120
//   bits  378..389             position.yCoordinate.confidence       = 90
//   bit   390                  velocity CHOICE index                 = cartesianVelocity
//   bit   391                  cartesianVelocity zVelocity-absent flag
//   bits  392..406             velocity.xVelocity.value              = 1520
//   bits  407..413             velocity.xVelocity.confidence         = 127
//   bits  414..428             velocity.yVelocity.value              = 0
//   bits  429..435             velocity.yVelocity.confidence         = 127
//   bits  436..438             classification SEQUENCE-OF length determinant = 1 entry
//   bits  439..441             ObjectClass CHOICE index              = vehicleSubClass
//   bits  442..445             vehicleSubClass                       = 5 (passengerCar)
//   bits  446..452             classification[0].confidence          = 95
//   bits  453..463  octets     trailing ZERO bits — 11 of them, i.e. one whole spare
//                   56-57      octet beyond the 57 octets the content actually needs.
//
// --- The nine cases -----------------------------------------------------
//
// STRUCTURALLY MALFORMED (Disposition::kReject):
//
//   empty.uper                     0 B  — zero-length datagram; hits the codec's
//                                         explicit `len == 0` guard.
//   truncated-nominal.uper        29 B  — nominal[0 : N/2] = first 29 of 58 octets;
//                                         cut inside the ManagementContainer.
//                                         sha256 unchanged from the 7-case corpus.
//   truncated-mid-object.uper     52 B  — nominal[0 : 52]. NEW. Cut deliberately
//     INSIDE the PerceivedObject rather than halfway through the PDU: bit 415 lands
//     in `velocity.yVelocity.value` (bits 414..428), so the decoder has already read
//     the PerceivedObject presence bitmap promising `velocity` AND `classification`,
//     has committed to the cartesianVelocity CHOICE arm, and then runs out of bits
//     mid-field with four elements still owed. The remaining
//     classification determinant / CHOICE index / subclass / confidence
//     (bits 436..452) do not exist at all. That is a short read, not a value edit.
//     first 8 = 02 0e 00 00 04 b1 02 9a, last 8 = 59 80 1e 01 66 8b df f9,
//     sha256 f4cf94c34734ed63cc9b009dc3a370d459ee895e258c7df3d0258ba7c617b974.
//   bit-flipped-payload.uper      58 B  — nominal with octet 36 inverted:
//     0x80 -> 0x7f (`b[36] ^= 0xFF`; octet 36 only — verified the other 57 are
//     byte-identical to nominal). NEW. OFFSET CHOICE IS THE WHOLE POINT: a flip
//     inside a plain constrained INTEGER's value bits usually yields a legal-but-
//     different value that decodes FINE (e.g. a different x-coordinate), which would
//     make this case a tolerated control by accident. Octet 36 = bits 288..295 lies
//     strictly inside the bits 255..300 span mapped above, which contains ONLY
//     structural elements — the container identifier, the containerData CHOICE index,
//     numberOfPerceivedObjects, the perceivedObjects length determinant and the
//     PerceivedObject OPTIONAL-presence bitmap. Inverting eight of those bits turns
//     absent OPTIONAL fields present (each then demanding bits that do not exist in a
//     58-octet buffer) and/or inflates a length determinant — a structural failure by
//     construction. Even in the sub-case where the resulting shape is readable, the
//     codec's own decode guards ("carries no objectId" / "no cartesian velocity" /
//     "no classification") reject it. Both routes count decode_reject.
//     first 8 = 02 0e 00 00 04 b1 02 9a, last 8 = df f9 ff ff e0 16 f0 00,
//     sha256 37c7246d26669b4452a04b8c496ba51c131e54c3ceced989c3fa8086763642f1.
//   random.uper                   58 B  — random.Random(20260801).randbytes(58);
//                                         first eight `97 c2 fa 8d bd 13 56 b4`.
//   oversized.uper              4096 B  — random.Random(20260802).randbytes(4096);
//                                         first eight `7a a3 9b ca 21 35 af 39`.
//                                         4 KiB is far beyond any legal profiled
//                                         CPM (58 B), so this case exercises the
//                                         size path as well as the content path.
//
// PROFILE-TOLERATED NEGATIVE CONTROLS (Disposition::kToleratedControl):
//
//   wrong-message-id.uper         58 B  — nominal with octet 1: 0x0e -> 0x02
//                                         (cpm(14) -> cam(2)).
//   wrong-protocol-version.uper   58 B  — nominal with octet 0: 0x02 -> 0x03
//                                         (release 2 -> a non-existent release 3).
//   r1-variant.uper               58 B  — nominal with octet 0: 0x02 -> 0x01,
//                                         the release-1 protocolVersion.
//   trailing-garbage.uper         66 B  — a COMPLETE, unmodified nominal.uper
//     followed by the 8 junk octets `de ad be ef de ad be ef`. NEW, and predicted
//     TOLERATED rather than rejected — encoding the honest expectation, not the
//     wished-for one. Vanetza decodes through `uper_decode_complete`
//     (vanetza/asn1/asn1c_wrapper.cpp -> vanetza/asn1/support/uper_decoder.c), which
//     returns RC_OK on the first complete PDU and never checks for unconsumed
//     trailing octets. The bit map above shows nominal.uper ALREADY carries one whole
//     spare zero octet past its last content bit (452) and still decodes green in the
//     golden-vector suite — direct evidence in this very tree that trailing octets are
//     ignored. So the surviving payload is byte-identical nominal and the decoded
//     CpmContent equals the nominal content, which is what the assertion checks.
//     first 8 = 02 0e 00 00 04 b1 02 9a, last 8 = de ad be ef de ad be ef,
//     sha256 1697cebae685054bdf978ae44a3ba5b65d016d57e1853ea1deb727ae0fb6e513.
//
// Two construction choices need stating, because the obvious alternatives do
// NOT reject and would have made silent pass-through look like success:
//
//  (1) oversized.uper is fixed-seed random, NOT "nominal + 0x00 padding to
//      4 KiB" — for exactly the `uper_decode_complete` reason spelled out under
//      trailing-garbage.uper above. A padded golden vector would be forwarded,
//      not rejected; trailing-garbage.uper is now that behaviour asserted
//      head-on as its own case instead of being merely avoided here.
//
//  (2) r1-variant.uper edits only protocolVersion. The release-1 and release-2
//      ItsPduHeader have the identical wire shape (8 + 8 + 32 bits), so
//      protocolVersion is the only header-level release marker there is; the
//      release-1 CpmPayload *structure* differs, but this tree cannot produce
//      release-1 bytes (convention F2 bans the bare release-1 wrapper under
//      V2X_ECU/src/, and no release-1 encoder exists here). Honest label: this
//      fixture is a release-1-labelled header carrying release-2 payload bytes —
//      which is why it belongs with the tolerated controls.

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

// What the frozen contracts REQUIRE of a corpus case. Asserted exactly — see the
// two-categories note at the top of this file.
enum class Disposition {
  kReject,            // structurally malformed: DecodeError, no sink call, counted
  kToleratedControl,  // valid profiled CPM: decodes, forwards, content == nominal
};

const char* Name(Disposition disposition) {
  return disposition == Disposition::kReject ? "Reject" : "ToleratedControl";
}

struct CorpusCase {
  const char* stem;           // file name without the .uper extension
  Disposition disposition;    // what the frozen contracts require of this input
  const char* category;       // human category, surfaced in failure output
  const char* why;            // one-line rationale, surfaced in failure output
};

constexpr char kMalformed[] = "structurally malformed";
constexpr char kTolerated[] = "profile-tolerated negative control";

// THE DISPOSITION TABLE — single source of truth. The whole-corpus test feeds
// the cases in this order and DERIVES its expected counters from it (see
// kRejectCases / kToleratedCases), so relabelling a case cannot silently desync
// the totals.
constexpr CorpusCase kCorpus[] = {
    {"empty", Disposition::kReject, kMalformed, "zero-length datagram"},
    {"truncated-nominal", Disposition::kReject, kMalformed,
     "first 29 of the golden 58 octets: short read in the management container"},
    {"truncated-mid-object", Disposition::kReject, kMalformed,
     "first 52 of 58 octets: cut inside velocity.yVelocity.value, classification missing"},
    {"bit-flipped-payload", Disposition::kReject, kMalformed,
     "octet 36 inverted: inside the PerceivedObject presence bitmap / length determinants"},
    {"random", Disposition::kReject, kMalformed, "fixed-seed random 58 octets"},
    {"oversized", Disposition::kReject, kMalformed, "4 KiB of fixed-seed random octets"},
    {"wrong-message-id", Disposition::kToleratedControl, kTolerated,
     "messageId cam(2): ignored on decode per profile section 3"},
    {"wrong-protocol-version", Disposition::kToleratedControl, kTolerated,
     "protocolVersion 3: ignored on decode per profile section 3"},
    {"r1-variant", Disposition::kToleratedControl, kTolerated,
     "protocolVersion 1: ignored on decode per profile section 3"},
    {"trailing-garbage", Disposition::kToleratedControl, kTolerated,
     "complete nominal + 8 junk octets: uper_decode_complete ignores unconsumed octets"},
};
constexpr std::size_t kCorpusSize = sizeof(kCorpus) / sizeof(kCorpus[0]);

// Counters DERIVED from the table above, never hand-maintained.
constexpr std::size_t CountDisposition(Disposition want) {
  std::size_t count = 0;
  for (const CorpusCase& c : kCorpus) {
    if (c.disposition == want) {
      ++count;
    }
  }
  return count;
}

constexpr std::size_t kRejectCases = CountDisposition(Disposition::kReject);
constexpr std::size_t kToleratedCases = CountDisposition(Disposition::kToleratedControl);
static_assert(kRejectCases + kToleratedCases == kCorpusSize,
              "every corpus case needs exactly one of the two dispositions");
static_assert(kRejectCases > 0 && kToleratedCases > 0,
              "the corpus must keep both categories: rejects prove R9, controls prove "
              "the suite is not passing by rejecting everything");

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

// Assertion-free counter read for the diagnostic path — a missing or non-integral
// key reports 0 rather than failing, so Diagnose() can never itself throw or add
// noise on top of the failure it is explaining.
std::uint64_t Counter(const nlohmann::json& counters, const char* key) {
  if (!counters.contains(key) || !counters.at(key).is_number_integer()) {
    return 0;
  }
  return counters.at(key).get<std::uint64_t>();
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

  // --- Diagnostics -------------------------------------------------------
  //
  // A wrong PREDICTION must read as one line, not as a re-investigation: the
  // dispositions in kCorpus are reasoned, not locally measured (see the header
  // note), so the first CI run may disagree with one of them. Observe() is
  // deliberately assertion-free so it can be called from a failure message.

  struct Observed {
    bool any_event = false;
    bool decoded = false;
    bool forwarded = false;
    std::uint64_t decode_reject = 0;
    std::uint64_t validate_reject = 0;
    std::uint64_t dedupe_drop = 0;
    std::string last_event;
    std::string detail;  // reject reason, empty when nothing was rejected
  };

  Observed Observe() const {
    Observed observed;
    for (const std::string& line : EvtLines()) {
      if (line.rfind(kPrefix, 0) != 0) {
        continue;
      }
      const nlohmann::json evt =
          nlohmann::json::parse(line.substr(kPrefixLen), nullptr, /*allow_exceptions=*/false);
      if (evt.is_discarded() || !evt.is_object()) {
        continue;
      }
      observed.any_event = true;
      if (evt.contains("event") && evt.at("event").is_string()) {
        observed.last_event = evt.at("event").get<std::string>();
        // The disposition gate reads the EVENT NAMES, not the counters: a stage
        // event is emitted exactly once per datagram it applies to, and this stays
        // correct whatever numeric flavour the counters serialise as.
        if (observed.last_event == "decode_ok") {
          observed.decoded = true;
        } else if (observed.last_event == "r2_forwarded") {
          observed.forwarded = true;
        }
      }
      if (evt.contains("detail") && evt.at("detail").is_string()) {
        observed.detail = evt.at("detail").get<std::string>();
      }
      if (evt.contains("counters") && evt.at("counters").is_object()) {
        const nlohmann::json& c = evt.at("counters");
        observed.decode_reject = Counter(c, "decode_reject");
        observed.validate_reject = Counter(c, "validate_reject");
        observed.dedupe_drop = Counter(c, "dedupe_drop");
      }
    }
    return observed;
  }

  std::string Diagnose(const CorpusCase& c) const {
    const Observed o = Observe();
    std::ostringstream d;
    d << "\n=== DISPOSITION MISMATCH — relabel this one case and re-run ==="
      << "\n  case             : " << c.stem << ".uper (" << c.category << ")"
      << "\n  expected         : " << Name(c.disposition) << " — " << c.why
      << "\n  decode           : " << (o.decoded ? "SUCCEEDED" : "did NOT succeed")
      << "\n  forwarded to R2  : " << (o.forwarded ? "YES" : "no")
      << "\n  reject counters  : decode_reject=" << o.decode_reject
      << "  validate_reject=" << o.validate_reject << "  dedupe_drop=" << o.dedupe_drop
      << "\n  last event       : " << (o.any_event ? o.last_event : "(no [EVT] line)")
      << "\n  reject detail    : " << (o.detail.empty() ? "(none)" : o.detail)
      << "\n  FIX: set this case's Disposition in kCorpus to "
      << Name(o.decoded ? Disposition::kToleratedControl : Disposition::kReject)
      << " and update the provenance note above with the measured reason."
      << "\n  full [EVT] log   :\n"
      << log_out_.str();
    return d.str();
  }
};

class MalformedCorpusTest : public MalformedPipelineTest,
                            public ::testing::WithParamInterface<CorpusCase> {};

// One datagram, one REQUIRED disposition, zero crashes. onDatagram is noexcept,
// so an escaping exception would std::terminate rather than propagate —
// surviving to the assertions is the proof; ExpectNoUnexpectedException adds that
// the codec did not throw internally either.
TEST_P(MalformedCorpusTest, IsHandledExactlyAsTheContractsRequire) {
  const CorpusCase& c = GetParam();
  SCOPED_TRACE(std::string(c.stem) + ".uper — " + c.category + " — " + c.why);

  const std::vector<std::uint8_t> bytes = ReadMalformed(c.stem);
  pipeline_.onDatagram(bytes);

  const auto lines = EvtLines();
  ASSERT_FALSE(lines.empty());
  const nlohmann::json first = ParseEvtLine(lines.front());
  EXPECT_EQ(first.at("event").get<std::string>(), "rx_datagram");
  EXPECT_EQ(first.at("bytes").get<std::size_t>(), bytes.size());
  ExpectNoUnexpectedException();

  // The disposition gate: no "either outcome passes" branch. The observed
  // disposition is decided by whether stage 1 produced a CpmContent at all.
  const Observed observed = Observe();
  const Disposition actual =
      observed.decoded ? Disposition::kToleratedControl : Disposition::kReject;
  ASSERT_STREQ(Name(c.disposition), Name(actual)) << Diagnose(c);

  if (c.disposition == Disposition::kReject) {
    // Sink never called, one decode_reject counted, nothing forwarded.
    EXPECT_TRUE(forwarded_.empty()) << Diagnose(c);
    ExpectCounters(/*rx=*/1, /*decode_ok=*/0, /*decode_reject=*/1, /*validate_reject=*/0,
                   /*dedupe_drop=*/0, /*r2_forwarded=*/0);
    // Two lines only: rx_datagram then decode_reject — the datagram never
    // reached stage 2, so no decode_ok appears in between.
    ASSERT_EQ(lines.size(), 2u) << Diagnose(c);
    const nlohmann::json reject = ParseEvtLine(lines.back());
    EXPECT_EQ(reject.at("event").get<std::string>(), "decode_reject");
    // A human-readable reason is part of the R9 "rejected AND logged" acceptance.
    EXPECT_FALSE(reject.at("detail").get<std::string>().empty());
    return;
  }

  // kToleratedControl: the differing octets are discarded by the frozen
  // contract — the three header edits by profile §3's ignore-on-decode rule, and
  // trailing-garbage.uper because its surviving payload IS nominal.uper — so the
  // decoded content, and the R2 message built from it, must be the golden
  // nominal one. That equality is the rule under test, not a side effect.
  ASSERT_EQ(forwarded_.size(), 1u) << Diagnose(c);
  ExpectNominalR2(forwarded_.front());
  ExpectCounters(/*rx=*/1, /*decode_ok=*/1, /*decode_reject=*/0, /*validate_reject=*/0,
                 /*dedupe_drop=*/0, /*r2_forwarded=*/1);
  // rx_datagram -> decode_ok -> r2_forwarded, nothing else: no reject and no
  // dedupe drop may hide between them.
  ASSERT_EQ(lines.size(), 3u) << Diagnose(c);
  EXPECT_EQ(ParseEvtLine(lines[1]).at("event").get<std::string>(), "decode_ok");
  EXPECT_EQ(ParseEvtLine(lines[2]).at("event").get<std::string>(), "r2_forwarded");
}

// gtest test-name suffixes must be alphanumeric/underscore, so the hyphenated
// case names are transliterated — same convention as the golden-vector suite.
std::string CaseName(const ::testing::TestParamInfo<CorpusCase>& info) {
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
// Every expected total is derived from the disposition table (kRejectCases /
// kToleratedCases), so relabelling a case updates the arithmetic automatically
// and cannot leave this test silently out of sync.
//
// The dedupe clock steps a full window between datagrams: all tolerated cases
// decode to identical content (same stationId/objectId/measurement time), so
// without the step they would collapse into dedupe_drops and hide the forwarding
// behavior under test.
TEST_F(MalformedPipelineTest, WholeCorpusIsSurvivedAndLeavesThePipelineUsable) {
  for (const CorpusCase& c : kCorpus) {
    SCOPED_TRACE(std::string("corpus case ") + c.stem + " (" + Name(c.disposition) + ")");
    pipeline_.onDatagram(ReadMalformed(c.stem));
    dedupe_clock_ms_ += kDedupeWindowMs;
  }

  ASSERT_EQ(forwarded_.size(), kToleratedCases) << log_out_.str();
  ExpectCounters(/*rx=*/kCorpusSize, /*decode_ok=*/kToleratedCases,
                 /*decode_reject=*/kRejectCases, /*validate_reject=*/0,
                 /*dedupe_drop=*/0, /*r2_forwarded=*/kToleratedCases);
  ExpectNoUnexpectedException();

  // Every tolerated control decodes to the same golden content, so all of them
  // are checked, not just the last one.
  for (const R2Message& r2 : forwarded_) {
    ExpectNominalR2(r2);
  }

  // Not wedged: the frozen golden vector still decodes and forwards through the
  // very same codec and stage instances that just swallowed 4 KiB of garbage.
  pipeline_.onDatagram(ReadFixture("golden/nominal.uper"));

  ASSERT_EQ(forwarded_.size(), kToleratedCases + 1) << log_out_.str();
  ExpectNominalR2(forwarded_.back());
  ExpectCounters(/*rx=*/kCorpusSize + 1, /*decode_ok=*/kToleratedCases + 1,
                 /*decode_reject=*/kRejectCases, /*validate_reject=*/0,
                 /*dedupe_drop=*/0, /*r2_forwarded=*/kToleratedCases + 1);
  ExpectNoUnexpectedException();
}

}  // namespace
