#pragma once

#include <optional>

#include "ada/track_store.hpp"

namespace ada {

struct SceneGeometry {
    Position ego;
    Position vehicle_b;
    std::optional<Position> vehicle_c;
    double distance_ac_m = 0.0;
};

std::optional<SceneGeometry> compose_scene(
    const TrackStore& store,
    const std::optional<TrackedObject>& relayed = std::nullopt);

}  // namespace ada
