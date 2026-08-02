#pragma once

#include <cstdint>
#include <string>

namespace ada {

enum class Source {
    OwnSensor,
    V2xRelayed,
};

enum class TrackState {
    NotTracked,
    Tentative,
    Tracked,
};

struct Position {
    double x_m = 0.0;
    double y_m = 0.0;
    double confidence = 0.0;
};

struct Timestamps {
    std::int64_t measured_ms = 0;
    std::int64_t received_ms = 0;
    std::int64_t last_updated_ms = 0;
};

struct TrackedObject {
    std::string id;
    std::string object_class = "vehicle";
    Source source = Source::OwnSensor;
    Position position;
    double distance_m = 0.0;
    double speed_mps = 0.0;
    double confidence = 0.0;
    TrackState state = TrackState::NotTracked;
    Timestamps timestamps;
};

inline const char* to_string(Source source) {
    return source == Source::OwnSensor ? "own_sensor" : "v2x_relayed";
}

inline const char* to_string(TrackState state) {
    switch (state) {
        case TrackState::NotTracked:
            return "not_tracked";
        case TrackState::Tentative:
            return "tentative";
        case TrackState::Tracked:
            return "tracked";
    }
    return "not_tracked";
}

}  // namespace ada

