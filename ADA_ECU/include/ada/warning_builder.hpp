#pragma once

#include <string>

#include "ada/risk_assessor.hpp"
#include "ada/track_store.hpp"

namespace ada {

std::string build_r4_warning_json(const RiskEvent& event, const TrackStore& store);

}  // namespace ada

