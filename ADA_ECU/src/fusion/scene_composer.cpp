#include "fusion/scene_composer.hpp"

namespace ada::fusion {

std::optional<SceneGeometry> compose(const store::TrackStore& store,
                                     const std::optional<contracts::TrackedObject>& trackedC,
                                     const std::optional<contracts::Vec2>& fallbackB) {
  SceneGeometry geometry;
  geometry.ego = contracts::Vec2{0.0, 0.0};

  const std::optional<contracts::TrackedObject> nearestB =
      store.nearest(contracts::Source::own_sensor);
  if (nearestB) {
    geometry.vehicleB = contracts::Vec2{nearestB->distance, nearestB->position.y};
  } else if (fallbackB) {
    // The assessment record's remembered (d_AB, y_B) — composition still
    // works on a clearing event (D5).
    geometry.vehicleB = *fallbackB;
  } else {
    // No own-sensor B and nothing remembered: d_AC does not exist (D5).
    return std::nullopt;
  }

  if (trackedC) {
    geometry.vehicleC = contracts::Vec2{geometry.vehicleB.x + trackedC->distance,
                                        geometry.vehicleB.y + trackedC->position.y};
  }
  return geometry;
}

}  // namespace ada::fusion
