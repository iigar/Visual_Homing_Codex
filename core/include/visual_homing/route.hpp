#pragma once

#include <cstddef>
#include <vector>

#include "visual_homing/time.hpp"

namespace vh {

struct RouteMatch {
    Timestamp timestamp{};
    std::size_t route_index = 0;
    double progress = 0.0;
    double direction_error_rad = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

struct RouteMatchCandidate {
    std::size_t route_index = 0;
    double progress = 0.0;
    double confidence = 0.0;
};

struct RouteMatchZoneCandidate {
    const char* name = "";
    double start_progress = 0.0;
    double end_progress = 0.0;
    RouteMatchCandidate candidate{};
    bool valid = false;
};

} // namespace vh
