#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ada/ada_pipeline.hpp"
#include "ada/config.hpp"
#include "ada/event_logger.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"

namespace {

class OneShotRisk final : public ada::CollisionRiskAssessor {
public:
    explicit OneShotRisk(ada::RiskEvent event) : event_(std::move(event)) {}

    std::optional<ada::RiskEvent> assess(const ada::TrackStore&, std::int64_t) override {
        auto result = event_;
        event_.reset();
        return result;
    }

private:
    std::optional<ada::RiskEvent> event_;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

ada::TrackedObject tracked(std::string id, ada::Source source, double distance) {
    ada::TrackedObject object;
    object.id = std::move(id);
    object.source = source;
    object.position = {distance, 0.0, 0.9};
    object.distance_m = distance;
    object.confidence = 0.9;
    object.state = ada::TrackState::Tracked;
    object.timestamps = {1000, 1000, 1000};
    return object;
}

ada::RiskEvent event_for(const ada::TrackedObject& c) {
    return {
        ada::RiskState::High,
        c,
        37.0,
        2.0,
        {{0.0, 0.0}, {12.0, 0.0}, ada::Position{49.0, 0.0, 0.9}, 37.0},
        true,
        "test_risk",
    };
}

}  // namespace

int main() {
    ada::AdaConfig config;
    config.tentative_hits = 1;
    config.ivi_host = "10.99.0.13";
    config.ivi_port = 47300;

    const auto b = tracked("own:B", ada::Source::OwnSensor, 12.0);
    const auto c = tracked("v2x:1201:7", ada::Source::V2xRelayed, 25.0);

    const auto success_log = std::filesystem::temp_directory_path() / "ada_pipeline_success.jsonl";
    std::filesystem::remove(success_log);
    ada::EventLogger success_logger(success_log.string());
    ada::TrackStore success_store(config);
    success_store.upsert(b);
    success_store.upsert(c);
    OneShotRisk success_risk(event_for(c));
    std::string sent_payload;
    ada::AdaPipeline success_pipeline(
        config,
        success_store,
        success_risk,
        success_logger,
        [&](const std::string& payload) { sent_payload = payload; });
    assert(success_pipeline.assess_and_emit(1100));
    assert(!sent_payload.empty());
    const auto first_payload = sent_payload;
    assert(!success_pipeline.assess_and_emit(1101));
    assert(sent_payload == first_payload);
    const auto success_events = read_file(success_log);
    assert(success_events.find("\"event\":\"assessment\"") != std::string::npos);
    assert(success_events.find("\"event\":\"risk_transition\"") != std::string::npos);
    assert(success_events.find("\"sent\":true") != std::string::npos);

    const auto failure_log = std::filesystem::temp_directory_path() / "ada_pipeline_failure.jsonl";
    std::filesystem::remove(failure_log);
    ada::EventLogger failure_logger(failure_log.string());
    ada::TrackStore failure_store(config);
    failure_store.upsert(b);
    failure_store.upsert(c);
    OneShotRisk failure_risk(event_for(c));
    ada::AdaPipeline failure_pipeline(
        config,
        failure_store,
        failure_risk,
        failure_logger,
        [](const std::string&) { throw std::runtime_error("simulated send failure"); });
    assert(failure_pipeline.assess_and_emit(1200));
    const auto failure_events = read_file(failure_log);
    assert(failure_events.find("\"event\":\"r4_tx_failed\"") != std::string::npos);
    assert(failure_events.find("simulated send failure") != std::string::npos);
    assert(failure_events.find("\"sent\":false") != std::string::npos);

    assert(!failure_pipeline.ingest_own_sensor("{bad"));
    assert(!failure_pipeline.ingest_r2("{bad", 1300).accepted);

    std::filesystem::remove(success_log);
    std::filesystem::remove(failure_log);
    return 0;
}
