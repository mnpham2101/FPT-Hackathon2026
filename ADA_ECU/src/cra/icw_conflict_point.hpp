#ifndef ADA_CRA_ICW_CONFLICT_POINT_HPP
#define ADA_CRA_ICW_CONFLICT_POINT_HPP

#include <string>

namespace ada::cra {

enum class ICWRiskLevel {
    NONE = 0,
    INFO = 1,
    WARNING = 2,
    CRITICAL = 3
};

inline std::string to_string(ICWRiskLevel level) {
    switch (level) {
        case ICWRiskLevel::INFO: return "INFO";
        case ICWRiskLevel::WARNING: return "WARNING";
        case ICWRiskLevel::CRITICAL: return "CRITICAL";
        default: return "NONE";
    }
}

inline ICWRiskLevel risk_level_from_string(const std::string& str) {
    if (str == "INFO") return ICWRiskLevel::INFO;
    if (str == "WARNING") return ICWRiskLevel::WARNING;
    if (str == "CRITICAL") return ICWRiskLevel::CRITICAL;
    return ICWRiskLevel::NONE;
}

struct ICWConflictPoint {
    bool has_conflict{false};
    double ttc_sec{999.0};          // Time-to-collision in seconds
    double distance_m{999.0};       // Relative distance to conflict point in meters
    double separation_min_m{999.0}; // Predicted minimum distance at closest approach
};

} // namespace ada::cra

#endif // ADA_CRA_ICW_CONFLICT_POINT_HPP
