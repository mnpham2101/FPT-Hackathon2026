#pragma once

#include <optional>
#include <string>

#include "ada/types.hpp"

namespace ada {

std::optional<TrackedObject> tracked_object_from_r3_json(const std::string& json);
std::string tracked_object_to_r3_json(const TrackedObject& object);

}  // namespace ada
