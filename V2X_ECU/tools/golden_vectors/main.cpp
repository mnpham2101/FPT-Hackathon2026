// V2X_ECU/tools/golden_vectors/main.cpp — `gv_tool`, the generator of the frozen R1
// golden-vector corpus (subtask 1.0.2.4).
//
// BUILD-ONLY TOOL (HLD D3): gv_tool is developer/CI equipment. It is never installed
// into, invoked by, or shipped inside any node container image — the V2X ECU runtime
// links the codec seam, not this executable. Its only outputs are the committed
// `contracts/golden-vectors/<case>.json` + `<case>.uper` pairs.
//
// What it does, per case, in a fixed order:
//   1. build the CpmContent (nominal + exactly one delta),
//   2. encode it through the R1 seam (VanetzaCpmCodec) to ASN.1 UPER octets,
//   3. decode those octets back and assert encode→decode identity (operator==),
//   4. write <output-dir>/<case>.json and <output-dir>/<case>.uper.
//
// DETERMINISM IS THE ACCEPTANCE CRITERION: two runs into two directories must produce
// byte-identical trees (CI diffs them). Hence — no timestamps, no randomness, no
// iteration over unordered containers, no locale-sensitive formatting (every emitted
// value is an integer, and nlohmann's integer serializer is locale-independent).
//
// Chosen output format, fixed and never parameterised:
//   *.json — nlohmann `dump(2)`: 2-space indent, no ASCII escaping, keys in
//            nlohmann's default ordered-map order (lexicographic), plus exactly one
//            trailing '\n'. Written in binary mode so no host adds CRLF.
//   *.uper — the raw encoded octets, nothing else, binary mode.
//
// Usage: gv_tool <output-dir>          (output-dir is required; created if absent)
// Exit:  0 = all 6 cases encoded, round-tripped and written
//        1 = a case failed (encode threw, round-trip mismatched, or write failed)
//        2 = usage error (missing/extra argv)

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "codec/cpm_codec.hpp"
#include "codec/vanetza_cpm_codec.hpp"

namespace {

using v2x::codec::CpmContent;
using v2x::codec::DecodeError;
using v2x::codec::VanetzaCpmCodec;

// The §4 nominal CpmContent — normative source: contracts/r1-cpm-profile.md §4.
// Frozen fixture values, not tunables: they are the wire definition every consumer
// (bench encoder R11, V2X decoder R9, R2 binding) is checked against.
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

// The frozen 6-case corpus — normative source: contracts/r1-cpm-profile.md §7.
// Each case is the nominal content with ONLY the stated delta applied. These are
// frozen fixture definitions (a contract), NOT configurable tunables: adding,
// removing or re-valuing a case requires re-freezing §7 across every consumer.
// The array order is the generation order and is itself part of the determinism.
struct GoldenCase {
  const char* name;                  // file stem: <name>.json / <name>.uper
  void (*apply_delta)(CpmContent&);  // no-op for `nominal`
  const char* exercises;             // §7 rationale, kept next to the value
};

const GoldenCase kGoldenCases[] = {
    {"nominal", [](CpmContent&) {}, "the committed happy path"},
    {"mdt-max", [](CpmContent& c) { c.object.measurementDeltaTime = 2047; }, "F9 upper bound"},
    {"mdt-min", [](CpmContent& c) { c.object.measurementDeltaTime = -2047; },
     "F9 lower bound (profile bound, not wire -2048)"},
    {"conf-unavailable", [](CpmContent& c) { c.object.classConfidence = 101; },
     "F6 101 -> null"},
    {"gate-boundary",
     [](CpmContent& c) {
       c.object.position.x = 3000;
       c.object.position.y = 0;
     },
     "R13 admission seam: derived distance exactly 30,00 m"},
    {"coord-large",
     [](CpmContent& c) {
       c.object.position.x = 131071;
       c.object.position.y = -131072;
     },
     "CartesianCoordinateLarge bounds; feeds the O3 MTU/size budget"},
};

constexpr std::size_t kGoldenCaseCount = sizeof(kGoldenCases) / sizeof(kGoldenCases[0]);

// Binary write for both artefact kinds — text mode would let a host translate '\n'
// and break byte-identity across platforms.
bool WriteBinaryFile(const std::filesystem::path& path, const char* data, std::size_t size) {
  std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!out) {
    std::cerr << "gv_tool: cannot open " << path.string() << " for writing\n";
    return false;
  }
  if (size > 0) {
    out.write(data, static_cast<std::streamsize>(size));
  }
  out.close();
  if (!out) {
    std::cerr << "gv_tool: failed writing " << path.string() << "\n";
    return false;
  }
  return true;
}

// One case: build → encode → decode-and-assert-identity → write the pair.
bool EmitCase(const GoldenCase& gc, const VanetzaCpmCodec& codec,
              const std::filesystem::path& out_dir) {
  CpmContent content = NominalContent();
  gc.apply_delta(content);

  std::vector<std::uint8_t> encoded;
  try {
    encoded = codec.encode(content);
  } catch (const std::exception& error) {
    std::cerr << "gv_tool: case '" << gc.name << "' failed to encode: " << error.what() << "\n";
    return false;
  }

  // Encode->decode identity: the corpus is only usable as a contract fixture if the
  // seam reproduces the exact same CpmContent from the bytes it just produced.
  const auto decoded = codec.decode(encoded.data(), encoded.size());
  if (!std::holds_alternative<CpmContent>(decoded)) {
    std::cerr << "gv_tool: case '" << gc.name
              << "' failed to decode: " << std::get<DecodeError>(decoded).reason << "\n";
    return false;
  }
  if (!(std::get<CpmContent>(decoded) == content)) {
    std::cerr << "gv_tool: case '" << gc.name
              << "' round-trip mismatch: decoded CpmContent differs from the encoded one\n";
    return false;
  }

  const nlohmann::json document = content;
  const std::string json_text = document.dump(2) + "\n";
  if (!WriteBinaryFile(out_dir / (std::string(gc.name) + ".json"), json_text.data(),
                       json_text.size())) {
    return false;
  }
  if (!WriteBinaryFile(out_dir / (std::string(gc.name) + ".uper"),
                       reinterpret_cast<const char*>(encoded.data()), encoded.size())) {
    return false;
  }

  // Per-case CI evidence: deterministic (name + byte count only), no path, no timing.
  std::cout << gc.name << ": " << encoded.size() << " bytes\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: gv_tool <output-dir>\n"
              << "  Generates the frozen R1 golden-vector corpus "
                 "(contracts/r1-cpm-profile.md section 7)\n"
              << "  as <output-dir>/<case>.json + <case>.uper pairs. Build-only tool.\n";
    return 2;
  }

  const std::filesystem::path out_dir(argv[1]);
  std::error_code ec;
  std::filesystem::create_directories(out_dir, ec);
  if (ec) {
    std::cerr << "gv_tool: cannot create output directory " << out_dir.string() << ": "
              << ec.message() << "\n";
    return 1;
  }
  if (!std::filesystem::is_directory(out_dir)) {
    std::cerr << "gv_tool: output path " << out_dir.string() << " is not a directory\n";
    return 1;
  }

  const VanetzaCpmCodec codec;
  for (std::size_t i = 0; i < kGoldenCaseCount; ++i) {
    if (!EmitCase(kGoldenCases[i], codec, out_dir)) {
      return 1;
    }
  }

  std::cout << kGoldenCaseCount << " golden vectors generated\n";
  return 0;
}
