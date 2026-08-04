#include "cra/assessment_db.hpp"

namespace ada::cra {

void to_json(nlohmann::json& j, const AssessmentRecord& r) {
  j = nlohmann::json{
      {"trackId", r.trackId},
      {"warningType", r.warningType},
      {"riskState", r.riskState},
      {"riskStateEnteredMs", r.riskStateEnteredMs},
      {"firstSeenMs", r.firstSeenMs},
      {"lastUpdatedMs", r.lastUpdatedMs},
      {"distanceM", r.distanceM},
      {"previousDistanceM", r.previousDistanceM},
      {"closingRateMps", r.closingRateMps},
      {"ttcS", r.ttcS ? nlohmann::json(*r.ttcS) : nlohmann::json(nullptr)},
      {"lastSnapshot", r.lastSnapshot},
      {"lastKnownB", r.lastKnownB ? nlohmann::json(*r.lastKnownB) : nlohmann::json(nullptr)},
      {"emittedCount", r.emittedCount},
      {"rationale", r.rationale}};
}

void from_json(const nlohmann::json& j, AssessmentRecord& r) {
  // j.at(...) throws on missing keys (all fourteen are schema-required, the
  // nullable ones as explicit null); unknown extra keys are ignored for
  // additive evolution, the convention of the contracts bindings.
  j.at("trackId").get_to(r.trackId);
  j.at("warningType").get_to(r.warningType);
  j.at("riskState").get_to(r.riskState);
  j.at("riskStateEnteredMs").get_to(r.riskStateEnteredMs);
  j.at("firstSeenMs").get_to(r.firstSeenMs);
  j.at("lastUpdatedMs").get_to(r.lastUpdatedMs);
  j.at("distanceM").get_to(r.distanceM);
  j.at("previousDistanceM").get_to(r.previousDistanceM);
  j.at("closingRateMps").get_to(r.closingRateMps);
  const nlohmann::json& ttc = j.at("ttcS");
  r.ttcS = ttc.is_null() ? std::nullopt : std::optional<double>(ttc.get<double>());
  j.at("lastSnapshot").get_to(r.lastSnapshot);
  const nlohmann::json& b = j.at("lastKnownB");
  r.lastKnownB =
      b.is_null() ? std::nullopt : std::optional<contracts::Vec2>(b.get<contracts::Vec2>());
  j.at("emittedCount").get_to(r.emittedCount);
  j.at("rationale").get_to(r.rationale);
}

AssessmentDb::AssessmentDb(log::EventLog& eventLog) : eventLog_(eventLog) {}

std::optional<AssessmentRecord> AssessmentDb::get(const std::string& trackId,
                                                  const std::string& warningType) const {
  const auto it = records_.find(Key{trackId, warningType});
  if (it == records_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void AssessmentDb::upsert(const AssessmentRecord& record) {
  records_[Key{record.trackId, record.warningType}] = record;
  eventLog_.emit(log::events::kAssessment, nlohmann::json(record));
}

void AssessmentDb::erase(const std::string& trackId) {
  // Keys sharing a trackId are contiguous under the map's pair ordering.
  auto it = records_.lower_bound(Key{trackId, std::string()});
  while (it != records_.end() && it->first.first == trackId) {
    it = records_.erase(it);
  }
}

std::vector<AssessmentRecord> AssessmentDb::all() const {
  std::vector<AssessmentRecord> out;
  out.reserve(records_.size());
  for (const auto& [key, record] : records_) {
    (void)key;
    out.push_back(record);
  }
  return out;
}

}  // namespace ada::cra
