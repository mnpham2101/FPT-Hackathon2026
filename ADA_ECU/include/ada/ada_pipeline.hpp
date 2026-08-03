#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ada/config.hpp"
#include "ada/risk_assessor.hpp"
#include "ada/v2x_r2_ingest.hpp"

namespace ada {

class EventLogger;
class TrackStore;

class AdaPipeline {
public:
    using R4Send = std::function<void(const std::string&)>;

    AdaPipeline(
        const AdaConfig& config,
        TrackStore& store,
        CollisionRiskAssessor& risk,
        EventLogger& logger,
        R4Send send_r4);

    bool ingest_own_sensor(const std::string& line);
    V2xR2IngestResult ingest_r2(const std::string& payload, std::int64_t received_ms);
    bool assess_and_emit(std::int64_t now_ms);

private:
    const AdaConfig& config_;
    TrackStore& store_;
    CollisionRiskAssessor& risk_;
    EventLogger& logger_;
    R4Send send_r4_;
};

}  // namespace ada
