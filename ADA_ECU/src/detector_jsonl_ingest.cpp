#include "ada/detector_jsonl_ingest.hpp"

#include <fstream>
#include <stdexcept>

#include "ada/r3_mapper.hpp"

namespace ada {

bool ingest_own_sensor_jsonl_line(const std::string& line, TrackStore& store, EventLogger& logger) {
    logger.write("own_sensor_ingest", line);
    const auto object = tracked_object_from_r3_json(line);
    if (!object || object->source != Source::OwnSensor) {
        logger.write("parse_reject", "{\"source\":\"own_sensor\",\"reason\":\"invalid_r3\"}");
        return false;
    }

    const auto update = store.upsert(*object);
    const auto stored = store.get(object->id);
    logger.write(
        "track_transition",
        "{\"id\":\"" + object->id + "\",\"source\":\"" + std::string(to_string(object->source)) +
            "\",\"previous\":\"" + std::string(to_string(update.previous)) + "\",\"state\":\"" +
            std::string(to_string(update.current)) + "\",\"distance\":" + std::to_string(object->distance_m) +
            ",\"changed\":" + (update.changed ? "true" : "false") +
            ",\"object\":" + tracked_object_to_r3_json(stored ? *stored : *object) + "}");
    return true;
}

DetectorIngestResult ingest_own_sensor_jsonl_file(const std::string& path, TrackStore& store, EventLogger& logger) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open own-sensor JSONL: " + path);
    }

    DetectorIngestResult result;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        if (!ingest_own_sensor_jsonl_line(line, store, logger)) {
            ++result.rejected;
            continue;
        }
        ++result.accepted;
    }

    return result;
}

}  // namespace ada
