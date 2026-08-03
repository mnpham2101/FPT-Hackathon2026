#pragma once

#include <string>

#include "ada/event_logger.hpp"
#include "ada/track_store.hpp"

namespace ada {

struct DetectorIngestResult {
    int accepted = 0;
    int rejected = 0;
};

bool ingest_own_sensor_jsonl_line(const std::string& line, TrackStore& store, EventLogger& logger);
DetectorIngestResult ingest_own_sensor_jsonl_file(const std::string& path, TrackStore& store, EventLogger& logger);

}  // namespace ada
