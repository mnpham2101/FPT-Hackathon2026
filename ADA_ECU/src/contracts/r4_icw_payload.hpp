#ifndef ADA_CONTRACTS_R4_ICW_PAYLOAD_HPP
#define ADA_CONTRACTS_R4_ICW_PAYLOAD_HPP

#include "cra/icw_conflict_point.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace ada::contracts {

struct ICWWarningPayload {
    uint32_t warning_id{0};
    std::string warning_type{"ICW"};
    cra::ICWRiskLevel risk_level{cra::ICWRiskLevel::NONE};
    uint32_t target_object_id{0};
    double distance_m{0.0};
    double ttc_sec{0.0};
    uint64_t timestamp_ms{0};

    nlohmann::json to_json() const;
    static ICWWarningPayload from_json(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const ICWWarningPayload& p);
void from_json(const nlohmann::json& j, ICWWarningPayload& p);

} // namespace ada::contracts

#endif // ADA_CONTRACTS_R4_ICW_PAYLOAD_HPP
