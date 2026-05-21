#pragma once

#include <chrono>

namespace vh {

using Clock = std::chrono::steady_clock;
using Timestamp = Clock::time_point;

Timestamp now();
double milliseconds_between(Timestamp start, Timestamp end);

} // namespace vh
