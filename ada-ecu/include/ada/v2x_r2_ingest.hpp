#pragma once

#include <chrono>
#include <string>

#include "ada/event_logger.hpp"
#include "ada/track_store.hpp"
#include "ada/udp_r2_receiver.hpp"

namespace ada {

struct V2xR2IngestResult {
    bool accepted = false;
    bool timed_out = false;
    std::string track_id;
    std::string reason;
};

V2xR2IngestResult ingest_r2_payload(const std::string& payload, std::int64_t received_ms, TrackStore& store, EventLogger& logger);
V2xR2IngestResult ingest_one_r2_udp(UdpR2Receiver& receiver, std::chrono::milliseconds timeout, TrackStore& store, EventLogger& logger);

}  // namespace ada

