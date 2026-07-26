#include "ada/detector_jsonl_ingest.hpp"

#include <fstream>
#include <stdexcept>

#include "ada/r3_mapper.hpp"

namespace ada {

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

        logger.write("own_sensor_rx", line);
        const auto object = tracked_object_from_r3_json(line);
        if (!object || object->source != Source::OwnSensor) {
            ++result.rejected;
            logger.write("own_sensor_rejected", "{\"reason\":\"invalid_r3_own_sensor\"}");
            continue;
        }

        const auto update = store.upsert(*object);
        logger.write("track_transition", "{\"id\":\"" + object->id + "\",\"state\":\"" + std::string(to_string(update.current)) + "\"}");
        ++result.accepted;
    }

    return result;
}

}  // namespace ada

