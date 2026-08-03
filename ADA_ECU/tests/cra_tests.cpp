#include <cassert>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "ada/assessment_db.hpp"
#include "ada/risk_registry.hpp"

namespace {

class StubAssessor final : public ada::CollisionRiskAssessor {
public:
    std::optional<ada::RiskEvent> assess(const ada::TrackStore&, std::int64_t) override {
        return std::nullopt;
    }
};

template <typename Fn>
bool throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    ada::RiskRegistry registry;
    registry.add("stub", [] { return std::make_unique<StubAssessor>(); });
    assert(registry.create("stub"));
    assert(throws([&] { registry.add("stub", [] { return std::make_unique<StubAssessor>(); }); }));
    assert(throws([&] { registry.create("missing"); }));
    assert(registry.create_enabled(" stub ").size() == 1);
    assert(throws([&] { registry.create_enabled("stub,missing"); }));
    assert(throws([&] { registry.create_enabled("stub,stub"); }));
    assert(throws([&] { registry.create_enabled(""); }));

    ada::AssessmentDb db;
    ada::AssessmentRecord record{1000, "nlos_obstruction", "v2x:1201:7", 37.4,
                                 ada::RiskState::Medium, "composed_distance_threshold"};
    db.upsert(record);
    assert(db.get("nlos_obstruction", "v2x:1201:7"));
    assert(db.history().size() == 1);
    auto invalid = record;
    invalid.track_id.clear();
    assert(throws([&] { db.upsert(invalid); }));
    invalid = record;
    invalid.distance_m = -1.0;
    assert(throws([&] { db.upsert(invalid); }));
    invalid = record;
    invalid.timestamp_ms = -1;
    assert(throws([&] { db.upsert(invalid); }));
    invalid = record;
    invalid.timestamp_ms = 999;
    assert(throws([&] { db.upsert(invalid); }));
    invalid = record;
    invalid.distance_m = std::numeric_limits<double>::quiet_NaN();
    assert(throws([&] { db.upsert(invalid); }));
    db.erase("nlos_obstruction", "v2x:1201:7");
    assert(!db.get("nlos_obstruction", "v2x:1201:7"));

    return 0;
}
