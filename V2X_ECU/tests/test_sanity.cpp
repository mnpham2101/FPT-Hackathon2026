#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <vanetza/asn1/cpm.hpp>

#include <type_traits>

TEST(V2xToolchainSanity, ParsesJsonLiteral) {
  const auto j = nlohmann::json::parse(R"({"ok": true})");
  EXPECT_TRUE(j.at("ok").get<bool>());
}

// Toolchain proof only: the Vanetza release-2 CPM type compiles, links and default-constructs.
// No ASN.1 field is touched here - the message field mapping belongs to 1.0.2.3 / 2.0.3.1.
// F2 discipline: only the fully qualified release-2 type is ever named; the release-1 alias is
// banned project-wide and gated by 1.0.7.1.
TEST(V2xToolchainSanity, ConstructsRelease2Cpm) {
  static_assert(std::is_default_constructible<vanetza::asn1::r2::Cpm>::value,
                "release-2 CPM wrapper must be default-constructible");
  vanetza::asn1::r2::Cpm cpm;
  (void) cpm;
  SUCCEED();
}
