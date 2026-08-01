#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(AdaToolchainSanity, ParsesJsonLiteral) {
  const auto j = nlohmann::json::parse(R"({"ok": true})");
  EXPECT_TRUE(j.at("ok").get<bool>());
}
