#include "contracts/r4_icw_payload.hpp"

namespace ada::contracts {

nlohmann::json ICWWarningPayload::to_json() const {
    return nlohmann::json{
        {"warning_id", warning_id},
        {"warning_type", warning_type},
        {"risk_level", cra::to_string(risk_level)},
        {"target_object_id", target_object_id},
        {"distance_m", distance_m},
        {"ttc_sec", ttc_sec},
        {"timestamp_ms", timestamp_ms}
    };
}

ICWWarningPayload ICWWarningPayload::from_json(const nlohmann::json& j) {
    ICWWarningPayload p;
    p.warning_id = j.at("warning_id").get<uint32_t>();
    p.warning_type = j.value("warning_type", "ICW");
    p.risk_level = cra::risk_level_from_string(j.at("risk_level").get<std::string>());
    p.target_object_id = j.at("target_object_id").get<uint32_t>();
    p.distance_m = j.at("distance_m").get<double>();
    p.ttc_sec = j.at("ttc_sec").get<double>();
    p.timestamp_ms = j.at("timestamp_ms").get<uint64_t>();
    return p;
}

void to_json(nlohmann::json& j, const ICWWarningPayload& p) {
    j = p.to_json();
}

void from_json(const nlohmann::json& j, ICWWarningPayload& p) {
    p = ICWWarningPayload::from_json(j);
}

} // namespace ada::contracts
