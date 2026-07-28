#include "ada/track_store.hpp"

#include <algorithm>

namespace ada {

TrackStore::TrackStore(AdaConfig config) : config_(std::move(config)) {}

TrackUpdateResult TrackStore::upsert(const TrackedObject& object) {
    if (object.source == Source::OwnSensor) {
        return apply_own_sensor(object);
    }
    return apply_v2x_relayed(object);
}

void TrackStore::expire(std::int64_t now_ms) {
    for (auto& item : tracks_) {
        auto& track = item.second;
        if (track.state != TrackState::NotTracked &&
            now_ms - track.timestamps.last_updated_ms > config_.miss_limit_ms) {
            track.state = TrackState::NotTracked;
        }
    }
}

std::optional<TrackedObject> TrackStore::get(const std::string& id) const {
    const auto it = tracks_.find(id);
    if (it == tracks_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<TrackedObject> TrackStore::all() const {
    std::vector<TrackedObject> values;
    values.reserve(tracks_.size());
    for (const auto& item : tracks_) {
        values.push_back(item.second);
    }
    return values;
}

std::optional<TrackedObject> TrackStore::nearest(Source source) const {
    std::optional<TrackedObject> best;
    for (const auto& item : tracks_) {
        const auto& track = item.second;
        if (track.source != source || track.state == TrackState::NotTracked) {
            continue;
        }
        if (!best || track.distance_m < best->distance_m) {
            best = track;
        }
    }
    return best;
}

std::optional<TrackedObject> TrackStore::latest(Source source) const {
    std::optional<TrackedObject> best;
    for (const auto& item : tracks_) {
        const auto& track = item.second;
        if (track.source != source) {
            continue;
        }
        if (!best || track.timestamps.last_updated_ms > best->timestamps.last_updated_ms) {
            best = track;
        }
    }
    return best;
}

TrackUpdateResult TrackStore::apply_own_sensor(TrackedObject object) {
    const auto previous = get(object.id);
    const auto previous_state = previous ? previous->state : TrackState::NotTracked;

    const auto hits = ++tentative_hits_[object.id];
    object.state = hits >= config_.tentative_hits ? TrackState::Tracked : TrackState::Tentative;
    tracks_[object.id] = object;

    return {previous_state != object.state, previous_state, object.state, "own_sensor_update"};
}

TrackUpdateResult TrackStore::apply_v2x_relayed(TrackedObject object) {
    const auto previous = get(object.id);
    const auto previous_state = previous ? previous->state : TrackState::NotTracked;

    if (object.distance_m <= config_.gate_enter_m) {
        object.state = TrackState::Tracked;
    } else if (previous && previous->state == TrackState::Tracked && object.distance_m <= config_.gate_exit_m) {
        object.state = TrackState::Tracked;
    } else {
        object.state = TrackState::NotTracked;
    }

    tracks_[object.id] = object;
    return {previous_state != object.state, previous_state, object.state, "v2x_gate_update"};
}

}  // namespace ada
