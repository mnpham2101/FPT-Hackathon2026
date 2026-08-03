#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ada/risk_assessor.hpp"

namespace ada {

struct AssessmentRecord {
    std::int64_t timestamp_ms = 0;
    std::string plugin;
    std::string track_id;
    double distance_m = 0.0;
    RiskState risk_state = RiskState::Low;
    std::string rationale;
};

class AssessmentDb {
public:
    virtual ~AssessmentDb() = default;

    virtual std::optional<AssessmentRecord> get(const std::string& plugin, const std::string& track_id) const;
    virtual void upsert(const AssessmentRecord& record);
    virtual void erase(const std::string& plugin, const std::string& track_id);
    virtual const std::vector<AssessmentRecord>& history() const;

private:
    static std::string key(const std::string& plugin, const std::string& track_id);
    std::unordered_map<std::string, AssessmentRecord> records_;
    std::vector<AssessmentRecord> history_;
};

}  // namespace ada
