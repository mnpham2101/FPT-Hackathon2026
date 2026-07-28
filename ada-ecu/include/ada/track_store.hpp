#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ada/config.hpp"
#include "ada/types.hpp"

namespace ada {

struct TrackUpdateResult {
    bool changed = false;
    TrackState previous = TrackState::NotTracked;
    TrackState current = TrackState::NotTracked;
    std::string reason;
};

class TrackStore {
public:
    explicit TrackStore(AdaConfig config);

    TrackUpdateResult upsert(const TrackedObject& object);
    void expire(std::int64_t now_ms);

    std::optional<TrackedObject> get(const std::string& id) const;
    std::vector<TrackedObject> all() const;
    std::optional<TrackedObject> nearest(Source source) const;
    std::optional<TrackedObject> latest(Source source) const;

private:
    TrackUpdateResult apply_own_sensor(TrackedObject object);
    TrackUpdateResult apply_v2x_relayed(TrackedObject object);

    AdaConfig config_;
    std::unordered_map<std::string, TrackedObject> tracks_;
    std::unordered_map<std::string, int> tentative_hits_;
};

}  // namespace ada
