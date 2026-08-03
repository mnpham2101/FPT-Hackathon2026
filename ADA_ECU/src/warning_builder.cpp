#include "ada/warning_builder.hpp"

#include <nlohmann/json.hpp>

#include "contracts/r4_message.hpp"

namespace ada {
namespace {

contracts::TrackedObject contract_object(const TrackedObject& object) {
    return {
        object.id,
        object.object_class,
        object.source == Source::OwnSensor ? contracts::Source::own_sensor : contracts::Source::v2x_relayed,
        {object.position.x_m, object.position.y_m},
        object.distance_m,
        object.speed_mps,
        object.confidence,
        object.state == TrackState::Tracked
            ? contracts::TrackState::tracked
            : object.state == TrackState::Tentative ? contracts::TrackState::tentative
                                                    : contracts::TrackState::not_tracked,
        {object.timestamps.measured_ms, object.timestamps.received_ms, object.timestamps.last_updated_ms},
    };
}

}  // namespace

std::string build_r4_warning_json(const RiskEvent& event, const TrackStore& store) {
    std::vector<contracts::TrackedObject> tracked;
    for (const auto& object : store.all()) {
        if (object.state == TrackState::Tracked) {
            tracked.push_back(contract_object(object));
        }
    }
    if (!event.has_current_c) {
        tracked.push_back(contract_object(event.object));
    }

    contracts::R4WarningEvent warning{
        1,
        "warning",
        "nlos_obstruction",
        to_string(event.state),
        contract_object(event.object),
        tracked,
        {
            {event.geometry.ego.x_m, event.geometry.ego.y_m},
            {event.geometry.vehicle_b.x_m, event.geometry.vehicle_b.y_m},
            event.geometry.vehicle_c
                ? std::optional<contracts::Vec2>{{event.geometry.vehicle_c->x_m, event.geometry.vehicle_c->y_m}}
                : std::nullopt,
        },
    };
    return nlohmann::json(warning).dump();
}

}  // namespace ada
