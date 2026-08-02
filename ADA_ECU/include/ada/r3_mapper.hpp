#pragma once

#include <optional>
#include <string>

#include "ada/types.hpp"

namespace ada {

std::optional<TrackedObject> tracked_object_from_r3_json(const std::string& json);

}  // namespace ada

