#include "ada/warning_builder.hpp"

#include <sstream>

namespace ada {
namespace {

std::string tracked_object_json(const TrackedObject& object) {
    std::ostringstream out;
    out << "{\"id\":\"" << object.id << "\","
        << "\"class\":\"" << object.object_class << "\","
        << "\"source\":\"" << to_string(object.source) << "\","
        << "\"position\":{\"x\":" << object.position.x_m << ",\"y\":" << object.position.y_m
        << ",\"confidence\":" << object.position.confidence << "},"
        << "\"distance\":" << object.distance_m << ","
        << "\"speed\":" << object.speed_mps << ","
        << "\"confidence\":" << object.confidence << ","
        << "\"state\":\"" << to_string(object.state) << "\","
        << "\"timestamps\":{"
        << "\"measured\":" << object.timestamps.measured_ms << ","
        << "\"received\":" << object.timestamps.received_ms << ","
        << "\"lastUpdated\":" << object.timestamps.last_updated_ms << "}}";
    return out.str();
}

const char* r4_risk_state(RiskState state) {
    return state == RiskState::Warning ? "high" : "low";
}

}  // namespace

std::string build_r4_warning_json(const RiskEvent& event, const TrackStore& store) {
    const auto own_b = store.nearest(Source::OwnSensor);
    const auto d_ab = own_b ? own_b->distance_m : 0.0;
    const auto d_bc = event.object.distance_m;
    const auto c_x = d_ab + d_bc;
    const auto b_y = own_b ? own_b->position.y_m : 0.0;
    const auto c_y = b_y + event.object.position.y_m;

    std::ostringstream out;
    out << "{\"schemaVersion\":1,"
        << "\"type\":\"warning\","
        << "\"warningType\":\"nlos_obstruction\","
        << "\"riskState\":\"" << r4_risk_state(event.state) << "\","
        << "\"object\":" << tracked_object_json(event.object) << ","
        << "\"trackedObjects\":[";
    if (own_b) {
        out << tracked_object_json(*own_b) << ",";
    }
    out << tracked_object_json(event.object) << "],"
        << "\"geometry\":{"
        << "\"ego\":{\"x\":0,\"y\":0},"
        << "\"vehicleB\":{\"x\":" << d_ab << ",\"y\":" << b_y << "},"
        << "\"vehicleC\":{\"x\":" << c_x << ",\"y\":" << c_y << "}"
        << "}}";
    return out.str();
}

}  // namespace ada
