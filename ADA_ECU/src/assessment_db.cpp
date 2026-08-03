#include "ada/assessment_db.hpp"

#include <cmath>
#include <stdexcept>

namespace ada {
namespace {

void validate(const AssessmentRecord& record) {
    if (record.timestamp_ms < 0) {
        throw std::runtime_error("assessment timestampMs must be non-negative");
    }
    if (record.plugin.empty() || record.track_id.empty() || record.rationale.empty()) {
        throw std::runtime_error("assessment plugin, trackId, and rationale must be non-empty");
    }
    if (!std::isfinite(record.distance_m) || record.distance_m < 0.0) {
        throw std::runtime_error("assessment distance must be finite and non-negative");
    }
}

}  // namespace

std::string AssessmentDb::key(const std::string& plugin, const std::string& track_id) {
    return plugin + '\x1f' + track_id;
}

std::optional<AssessmentRecord> AssessmentDb::get(const std::string& plugin, const std::string& track_id) const {
    const auto found = records_.find(key(plugin, track_id));
    if (found == records_.end()) {
        return std::nullopt;
    }
    return found->second;
}

void AssessmentDb::upsert(const AssessmentRecord& record) {
    validate(record);
    const auto record_key = key(record.plugin, record.track_id);
    const auto existing = records_.find(record_key);
    if (existing != records_.end() && record.timestamp_ms < existing->second.timestamp_ms) {
        throw std::runtime_error("assessment timestampMs cannot move backwards");
    }
    records_[record_key] = record;
    history_.push_back(record);
}

void AssessmentDb::erase(const std::string& plugin, const std::string& track_id) {
    records_.erase(key(plugin, track_id));
}

const std::vector<AssessmentRecord>& AssessmentDb::history() const {
    return history_;
}

}  // namespace ada
