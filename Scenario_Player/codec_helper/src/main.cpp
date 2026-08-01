// Scenario_Player/codec_helper/src/main.cpp — `cpm_encode`, the bench's R1 encoder CLI
// (SP HLD D1). It is the ONLY file in this folder that is original to the Scenario Player:
// everything under src/codec/ and cmake/ is a byte-synced copy of the V2X ECU codec seam and must
// never be edited here (SP HLD D2; gated by contracts/check_sync.py).
//
// WHY A SUBPROCESS: the R1 codec is C++/Vanetza and the bench generator is Python, so the bench
// reaches the frozen codec over a process boundary instead of duplicating the encoding (SP HLD
// D1). The consumer is Scenario_Player/player/encoder_client.py.
//
// THE FROZEN D1 PROTOCOL (`--stream`), one message per line in each direction:
//   stdin  <- one compact CpmContent JSON per line
//   stdout -> one base64 (RFC 4648, standard alphabet, no wrapping) UPER payload per success,
//             or one compact {"error":"<reason>"} object per failure.
// An encode failure NEVER kills the stream: the error line is written and the loop continues, so
// the bench keeps ticking and only the offending message is lost. Clean EOF on stdin exits 0.
// The reply MUST be plain base64 with no prefix, suffix or embedded whitespace — the client
// calls base64.b64decode(line, validate=True), which rejects any unexpected character.
//
// `--encode <file>` is the one-shot form used by tests and CI (Scenario_Player/tests/
// test_encoder_golden.py, 11.1.7.2): it encodes a single CpmContent JSON file, prints the base64
// payload, and fails loudly with a non-zero exit instead of continuing.
//
// Usage:  cpm_encode --stream
//         cpm_encode --encode <cpm-content.json>
// Exit:   0 = stream reached EOF cleanly / the one-shot encode succeeded
//         1 = the one-shot encode failed (unreadable file, bad JSON, un-encodable content)
//         2 = usage error (no args, unknown args, --help)
//
// JSON SHAPE: the input is exactly what player/contracts/cpm_content.py's to_json emits — the
// same keys and nesting as contracts/r1-cpm-content.schema.json, in wire-native integer units.
// No mapping is re-derived here: parsing goes through the SYNCED seam's nlohmann from_json for
// CpmContent (src/codec/cpm_codec.hpp), so the two sides agree by construction.

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "codec/vanetza_cpm_codec.hpp"

namespace {

using v2x::codec::CpmContent;
using v2x::codec::VanetzaCpmCodec;

// RFC 4648 §4 "base64" alphabet: indices 0..25 = 'A'..'Z', 26..51 = 'a'..'z', 52..61 = '0'..'9',
// 62 = '+', 63 = '/'. This is the standard alphabet Python's base64.b64decode expects — NOT the
// URL-safe '-_' variant.
constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64-encode raw octets, standard alphabet, '=' padded, NO line wrapping.
//
// Three input bytes (24 bits) become four 6-bit symbols. A trailing partial group is
// zero-extended to the next 6-bit boundary and padded to four characters with '=':
//   1 leftover byte  (8 bits)  -> 2 symbols + "=="
//   2 leftover bytes (16 bits) -> 3 symbols + "="
// No newline or whitespace is ever inserted: the whole payload must survive
// base64.b64decode(..., validate=True) on the Python side.
std::string Base64Encode(const std::vector<std::uint8_t>& data) {
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);

  std::size_t i = 0;
  for (; i + 3 <= data.size(); i += 3) {
    const std::uint32_t group = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
    out.push_back(kBase64Alphabet[(group >> 18) & 0x3F]);
    out.push_back(kBase64Alphabet[(group >> 12) & 0x3F]);
    out.push_back(kBase64Alphabet[(group >> 6) & 0x3F]);
    out.push_back(kBase64Alphabet[group & 0x3F]);
  }

  const std::size_t remaining = data.size() - i;
  if (remaining == 1) {
    const std::uint32_t group = static_cast<std::uint32_t>(data[i]) << 16;
    out.push_back(kBase64Alphabet[(group >> 18) & 0x3F]);
    out.push_back(kBase64Alphabet[(group >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (remaining == 2) {
    const std::uint32_t group = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
    out.push_back(kBase64Alphabet[(group >> 18) & 0x3F]);
    out.push_back(kBase64Alphabet[(group >> 12) & 0x3F]);
    out.push_back(kBase64Alphabet[(group >> 6) & 0x3F]);
    out.push_back('=');
  }

  return out;
}

// One {"error":"<reason>"} line on stdout, compact and immediately flushed — the D1 failure
// reply. Built with nlohmann rather than string concatenation so quotes, backslashes and
// newlines inside `reason` (exception messages are free text) are escaped correctly and the
// one-line framing invariant holds. error_handler_t::replace keeps dump() from throwing if an
// exception message carries a byte that is not valid UTF-8.
void PrintErrorLine(const std::string& reason) {
  nlohmann::json error = nlohmann::json::object();
  error["error"] = reason;
  std::cout << error.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) << '\n'
            << std::flush;
}

// Parse one CpmContent JSON document and encode it to a base64 UPER payload.
//
// Throws on every failure mode, all deriving from std::exception: nlohmann::json::parse_error
// (malformed JSON), nlohmann::json::out_of_range / type_error (missing or wrongly-typed field, via
// the seam's from_json j.at(...)), std::out_of_range (the F9 |measurementDeltaTime| > 2047 profile
// bound) and std::runtime_error (Vanetza's asn1c PER wrapper rejecting an out-of-CDD-range field).
std::string EncodeToBase64(const std::string& json_text, const VanetzaCpmCodec& codec) {
  const nlohmann::json document = nlohmann::json::parse(json_text);
  const CpmContent content = document.get<CpmContent>();
  const std::vector<std::uint8_t> encoded = codec.encode(content);
  return Base64Encode(encoded);
}

// `--stream`: the persistent D1 loop. Reads until EOF and returns 0; a failing line yields one
// error reply and the loop continues (HLD D1 — an encode failure never kills the stream).
int RunStream(const VanetzaCpmCodec& codec) {
  std::string line;
  while (std::getline(std::cin, line)) {
    // A trailing '\r' (a CRLF-terminated writer) is not part of the JSON document, so drop it
    // before anything else; a line that was only "\r\n" then reads as empty.
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    // Blank lines are skipped silently rather than answered: they carry no message, so replying
    // would desynchronize the client's strict one-line-in/one-line-out framing. Mirrors the
    // committed fake helper (Scenario_Player/tests/fake_cpm_encode.py).
    if (line.empty()) {
      continue;
    }

    try {
      std::cout << EncodeToBase64(line, codec) << '\n' << std::flush;
    } catch (const std::exception& error) {
      PrintErrorLine(error.what());
    } catch (...) {
      PrintErrorLine("unknown error while encoding CpmContent");
    }
  }
  return 0;
}

// Read a whole file as bytes. Returns false if it cannot be opened or read.
bool ReadWholeFile(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) {
    return false;
  }
  out = buffer.str();
  return true;
}

// `--encode <file>`: one-shot for tests/CI. Prints the base64 payload and returns 0, or prints the
// same {"error":...} line on stdout plus a human message on stderr and returns 1.
int RunOneShot(const std::string& path, const VanetzaCpmCodec& codec) {
  std::string json_text;
  if (!ReadWholeFile(path, json_text)) {
    const std::string reason = "cannot read input file " + path;
    PrintErrorLine(reason);
    std::cerr << "cpm_encode: " << reason << "\n";
    return 1;
  }

  try {
    std::cout << EncodeToBase64(json_text, codec) << '\n' << std::flush;
  } catch (const std::exception& error) {
    PrintErrorLine(error.what());
    std::cerr << "cpm_encode: failed to encode " << path << ": " << error.what() << "\n";
    return 1;
  } catch (...) {
    const std::string reason = "unknown error while encoding CpmContent";
    PrintErrorLine(reason);
    std::cerr << "cpm_encode: failed to encode " << path << ": " << reason << "\n";
    return 1;
  }
  return 0;
}

void PrintUsage() {
  std::cerr << "usage: cpm_encode --stream\n"
            << "       cpm_encode --encode <cpm-content.json>\n"
            << "  --stream          persistent mode (SP HLD D1): one CpmContent JSON per stdin\n"
            << "                    line -> one base64 UPER line, or one {\"error\":...} line;\n"
            << "                    a bad line never terminates the stream.\n"
            << "  --encode <file>   one-shot: encode a single CpmContent JSON file to base64 on\n"
            << "                    stdout; non-zero exit on failure.\n";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string mode = argc >= 2 ? std::string(argv[1]) : std::string();

  // `--help` deliberately falls through to the usage/exit-2 path: this is machine equipment with
  // exactly two modes, and a helper started with the wrong argv must fail loudly, not print help
  // onto the stdout channel the client is reading.
  if (argc == 2 && mode == "--stream") {
    const VanetzaCpmCodec codec;
    return RunStream(codec);
  }
  if (argc == 3 && mode == "--encode") {
    const VanetzaCpmCodec codec;
    return RunOneShot(std::string(argv[2]), codec);
  }

  PrintUsage();
  return 2;
}
