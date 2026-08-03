#include "ada/scene_composer.hpp"

namespace ada {

std::optional<SceneGeometry> compose_scene(
    const TrackStore& store,
    const std::optional<TrackedObject>& relayed) {
    const auto own_b = store.nearest(Source::OwnSensor);
    if (!own_b) {
        return std::nullopt;
    }

    SceneGeometry scene;
    scene.vehicle_b = own_b->position;
    scene.vehicle_b.x_m = own_b->distance_m;
    if (relayed) {
        scene.vehicle_c = Position{
            own_b->distance_m + relayed->position.x_m,
            own_b->position.y_m + relayed->position.y_m,
            std::min(own_b->confidence, relayed->confidence),
        };
        scene.distance_ac_m = own_b->distance_m + relayed->distance_m;
    }
    return scene;
}

}  // namespace ada
