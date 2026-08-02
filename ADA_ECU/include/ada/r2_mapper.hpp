#pragma once

#include <optional>
#include <string>

#include "ada/types.hpp"

namespace ada {

std::optional<TrackedObject> tracked_object_from_r2_json(const std::string& json, std::int64_t received_ms);

}  // namespace ada

