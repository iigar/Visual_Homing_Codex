#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "visual_homing/rc_switch_trigger_decoder.hpp"

namespace {

vh::Timestamp at_milliseconds(std::int64_t milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(
        std::chrono::milliseconds(milliseconds)));
}

std::size_t parse_count(const char* text, const char* name) {
    const std::string value_text(text);
    if (value_text.empty()
        || value_text.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(std::string(name) + " must be a non-negative integer");
    }
    std::size_t parsed = 0;
    const auto value = std::stoull(value_text, &parsed, 10);
    if (parsed != value_text.size() || value > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(std::string(name) + " must be a non-negative integer");
    }
    return static_cast<std::size_t>(value);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: rc12_local_reset_dry_run <trace.txt>"
                     " [minimum_trigger_events] [maximum_trigger_events]\n";
        return 2;
    }

    try {
        const auto minimum_trigger_events = argc >= 3 ? parse_count(argv[2], "minimum") : 0;
        const auto maximum_trigger_events = argc >= 4
            ? parse_count(argv[3], "maximum")
            : std::numeric_limits<std::size_t>::max();
        if (minimum_trigger_events > maximum_trigger_events) {
            throw std::invalid_argument("minimum trigger count exceeds maximum");
        }

        std::ifstream input(argv[1]);
        if (!input) {
            throw std::runtime_error("failed to open RC12 trace");
        }

        vh::RcSwitchTriggerDecoder decoder;
        std::size_t samples = 0;
        std::size_t rejected_samples = 0;
        std::size_t trigger_events = 0;
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line.front() == '#') {
                continue;
            }
            std::istringstream fields(line);
            std::int64_t timestamp_ms = 0;
            unsigned int pwm = 0;
            std::string trailing;
            if (!(fields >> timestamp_ms >> pwm) || (fields >> trailing)
                || timestamp_ms < 0 || pwm > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("invalid RC12 trace line: " + line);
            }

            ++samples;
            const auto result = decoder.observe(vh::RcSwitchObservation{
                at_milliseconds(timestamp_ms),
                static_cast<std::uint16_t>(pwm),
            });
            if (!result.sample_accepted) {
                ++rejected_samples;
            }
            if (result.trigger_edge) {
                ++trigger_events;
            }
            std::cout
                << "rc12_local_reset_dry_run_sample"
                << " sample=" << samples
                << " time_boot_ms=" << timestamp_ms
                << " pwm=" << pwm
                << " sample_accepted=" << (result.sample_accepted ? "true" : "false")
                << " stable_position=" << vh::rc_switch_position_name(result.stable_position)
                << " low_armed=" << (result.low_armed ? "true" : "false")
                << " trigger_edge=" << (result.trigger_edge ? "true" : "false")
                << " decision="
                << (result.trigger_edge ? "would_request_local_estimator_reset" : "observe_only")
                << " executor_attached=false"
                << " fc_home_change_attached=false"
                << " reason=" << result.reason
                << '\n';
        }

        const bool trigger_count_valid = trigger_events >= minimum_trigger_events
            && trigger_events <= maximum_trigger_events;
        const bool passed = samples > 0 && rejected_samples == 0 && trigger_count_valid;
        std::cout
            << "rc12_local_reset_dry_run_done"
            << " passed=" << (passed ? "true" : "false")
            << " samples=" << samples
            << " rejected_samples=" << rejected_samples
            << " trigger_events=" << trigger_events
            << " expected_trigger_events=" << minimum_trigger_events << "..";
        if (maximum_trigger_events == std::numeric_limits<std::size_t>::max()) {
            std::cout << "unbounded";
        } else {
            std::cout << maximum_trigger_events;
        }
        std::cout
            << " executor_attached=false"
            << " fc_home_change_attached=false"
            << '\n';
        return passed ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "rc12_local_reset_dry_run_error reason=" << error.what() << '\n';
        return 2;
    }
}
