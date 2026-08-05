#ifndef ADA_ECU_FUSION_SCENE_COMPOSER_HPP
#define ADA_ECU_FUSION_SCENE_COMPOSER_HPP

// Ego-frame scene composition (HLD §6, Business logic, the
// fusion/scene_composer row; design decision D5): vehicleB = (d_AB, y_B) from
// the nearest own_sensor track, vehicleC = (d_AB + d_BC, y_B + y_BC) —
// longitudinal sum, lateral component-wise, valid for the near-collinear
// convoy (milestone1.md §2). ego is always the origin.
//
// Pure: a function of the store and the passed values — no env, no sockets,
// no logging (house rules). The caller passes C's stored entry only while its
// state is `tracked`, and the assessment record's remembered lastKnownB as
// the fallback:
//   - no own-sensor B but fallbackB present → composition uses the remembered
//     (d_AB, y_B), which is what lets a clearing event still compose (D5);
//   - neither → nullopt ("not composable") — the caller decides; D5's
//     b_unknown path;
//   - trackedC nullopt → vehicleC nullopt, the frozen R4 null case — before C
//     is first admitted and after its track is erased alike.

#include <optional>

#include "contracts/tracked_object.hpp"
#include "store/track_store.hpp"

namespace ada::fusion {

// The R4 geometry triple in the ego frame, metres.
struct SceneGeometry {
  contracts::Vec2 ego;                      // always {0, 0}
  contracts::Vec2 vehicleB;
  std::optional<contracts::Vec2> vehicleC;  // nullopt whenever C has no `tracked` entry
};

std::optional<SceneGeometry> compose(
    const store::TrackStore& store,
    const std::optional<contracts::TrackedObject>& trackedC,
    const std::optional<contracts::Vec2>& fallbackB);

}  // namespace ada::fusion

#endif  // ADA_ECU_FUSION_SCENE_COMPOSER_HPP
