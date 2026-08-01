#include "contracts/r4_message.hpp"

namespace ada::contracts {

void to_json(nlohmann::json& j, const R4VehicleSet& v) {
  j = nlohmann::json{{"ego", v.ego}, {"vehicleB", v.vehicleB}};
  // vehicleC key always emitted, null when nullopt — valid for both the
  // warning's required-nullable `geometry.vehicleC` and the state's
  // absent-or-null `vehicles.vehicleC` (see header comment).
  j["vehicleC"] = v.vehicleC.has_value() ? nlohmann::json(*v.vehicleC) : nlohmann::json(nullptr);
}

void from_json(const nlohmann::json& j, R4VehicleSet& v) {
  // j.at(...) throws on missing keys (schema-required); unknown extra keys
  // are deliberately ignored for additive evolution.
  j.at("ego").get_to(v.ego);
  j.at("vehicleB").get_to(v.vehicleB);
  // Missing key OR null both map to nullopt (state allows absence, warning
  // requires the key but allows null).
  const auto it = j.find("vehicleC");
  v.vehicleC = (it == j.end() || it->is_null()) ? std::nullopt
                                                : std::optional<Vec2>(it->get<Vec2>());
}

void to_json(nlohmann::json& j, const R4WarningEvent& w) {
  j = nlohmann::json{{"schemaVersion", w.schemaVersion},
                     {"type", w.type},
                     {"warningType", w.warningType},
                     {"riskState", w.riskState},
                     {"object", w.object},
                     {"geometry", w.geometry}};
}

void from_json(const nlohmann::json& j, R4WarningEvent& w) {
  j.at("schemaVersion").get_to(w.schemaVersion);
  j.at("type").get_to(w.type);
  j.at("warningType").get_to(w.warningType);
  j.at("riskState").get_to(w.riskState);
  j.at("object").get_to(w.object);
  j.at("geometry").get_to(w.geometry);
}

void to_json(nlohmann::json& j, const R4StateMessage& s) {
  j = nlohmann::json{{"schemaVersion", s.schemaVersion},
                     {"type", s.type},
                     {"seq", s.seq},
                     {"vehicles", s.vehicles}};
}

void from_json(const nlohmann::json& j, R4StateMessage& s) {
  j.at("schemaVersion").get_to(s.schemaVersion);
  j.at("type").get_to(s.type);
  j.at("seq").get_to(s.seq);
  j.at("vehicles").get_to(s.vehicles);
}

}  // namespace ada::contracts
