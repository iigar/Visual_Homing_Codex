#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "visual_homing/route_local_odometry_estimator.hpp"

namespace {

constexpr std::uint8_t kSourceSystem = 191;
constexpr std::uint8_t kSourceComponent = 197;

vh::Timestamp at_milliseconds(std::int64_t milliseconds) {
    return vh::Timestamp(std::chrono::duration_cast<vh::Clock::duration>(
        std::chrono::milliseconds(milliseconds)));
}

vh::RouteLocalOdometryEstimatorConfig sitl_config() {
    vh::RouteLocalOdometryEstimatorConfig config;
    config.nominal_route_length_m = 10.0;
    config.minimum_match_confidence = 0.9;
    config.maximum_match_age_ms = 100.0;
    config.maximum_altitude_age_ms = 100.0;
    config.minimum_update_interval_ms = 20.0;
    config.maximum_update_interval_ms = 500.0;
    config.maximum_horizontal_rate_mps = 5.0;
    config.maximum_vertical_rate_mps = 2.0;
    config.maximum_yaw_rate_rad_s = 1.0;
    config.maximum_direction_error_rad = 0.5;
    config.maximum_progress_direction_error = 0.02;
    config.maximum_consecutive_invalid_updates = 3;
    return config;
}

std::string to_hex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : bytes) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

vh::RouteLocalOdometryObservation observation(
    std::int64_t now_ms,
    double progress,
    double altitude_m,
    double direction_error_rad,
    bool health_ready,
    vh::RouteTravelDirection direction) {
    vh::RouteLocalOdometryObservation value;
    value.timestamp = at_milliseconds(now_ms);
    value.match.timestamp = at_milliseconds(now_ms);
    value.match.progress = progress;
    value.match.direction_error_rad = direction_error_rad;
    value.match.direction_observation_valid = true;
    value.match.confidence = 0.99;
    value.match.valid = true;
    value.altitude_timestamp = at_milliseconds(now_ms);
    value.altitude_m = altitude_m;
    value.altitude_valid = true;
    value.health_ready = health_ready;
    value.direction = direction;
    return value;
}

bool self_test() {
    vh::RouteLocalOdometryEstimator estimator(sitl_config());
    estimator.initialize_start_altitude(0.5, at_milliseconds(1000), at_milliseconds(1000));

    const auto first = estimator.update(observation(
        1050,
        0.0,
        0.5,
        0.0,
        true,
        vh::RouteTravelDirection::forward));
    if (!first.estimate.valid_for_fc || first.estimate.reset_counter != 0) {
        return false;
    }
    const auto first_frame = vh::encode_mavlink2_route_local_odometry(
        first.estimate,
        1050000ULL,
        0,
        kSourceSystem,
        kSourceComponent);
    if (first_frame.size() != 244 || first_frame.at(10 + 228) != 20 || first_frame.at(10 + 229) != 12) {
        return false;
    }

    estimator.reset_tracking();
    const auto after_reset = estimator.update(observation(
        1100,
        0.0,
        0.5,
        0.0,
        true,
        vh::RouteTravelDirection::forward));
    if (!after_reset.estimate.valid_for_fc || after_reset.estimate.reset_counter != 1) {
        return false;
    }
    const auto reset_frame = vh::encode_mavlink2_route_local_odometry(
        after_reset.estimate,
        1100000ULL,
        1,
        kSourceSystem,
        kSourceComponent);
    return reset_frame.size() == 244 && reset_frame.at(10 + 230) == 1;
}

vh::RouteTravelDirection parse_direction(const std::string& text) {
    if (text == "forward") {
        return vh::RouteTravelDirection::forward;
    }
    if (text == "reverse") {
        return vh::RouteTravelDirection::reverse;
    }
    throw std::invalid_argument("direction must be forward or reverse");
}

int interactive_main() {
    vh::RouteLocalOdometryEstimator estimator(sitl_config());
    std::uint8_t sequence = 0;
    std::string line;

    std::cout << "HELLO protocol=1 source_system=" << static_cast<unsigned int>(kSourceSystem)
              << " source_component=" << static_cast<unsigned int>(kSourceComponent) << '\n'
              << std::flush;

    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;

        try {
            if (command == "INIT") {
                std::int64_t now_ms = 0;
                double altitude_m = 0.0;
                if (!(input >> now_ms >> altitude_m)) {
                    throw std::invalid_argument("INIT requires now_ms altitude_m");
                }
                estimator.initialize_start_altitude(
                    altitude_m,
                    at_milliseconds(now_ms),
                    at_milliseconds(now_ms));
                std::cout << "READY reset=" << static_cast<unsigned int>(estimator.reset_counter()) << '\n';
            } else if (command == "UPDATE") {
                std::int64_t now_ms = 0;
                double progress = 0.0;
                double altitude_m = 0.0;
                double direction_error_rad = 0.0;
                int health_ready = 0;
                std::string direction_text;
                if (!(input >> now_ms >> progress >> altitude_m >> direction_error_rad
                    >> health_ready >> direction_text)) {
                    throw std::invalid_argument(
                        "UPDATE requires now_ms progress altitude_m direction_error_rad health_ready direction");
                }
                if (now_ms < 0 || (health_ready != 0 && health_ready != 1)) {
                    throw std::invalid_argument("UPDATE now_ms/health_ready is invalid");
                }
                const auto result = estimator.update(observation(
                    now_ms,
                    progress,
                    altitude_m,
                    direction_error_rad,
                    health_ready == 1,
                    parse_direction(direction_text)));
                if (!result.estimate.valid_for_fc) {
                    std::cout << "DROP reason=" << result.reason
                              << " reset=" << static_cast<unsigned int>(result.estimate.reset_counter)
                              << " reset_required=" << (result.reset_required ? 1 : 0)
                              << " invalid=" << result.consecutive_invalid_updates << '\n';
                } else {
                    const auto frame = vh::encode_mavlink2_route_local_odometry(
                        result.estimate,
                        static_cast<std::uint64_t>(now_ms) * 1000ULL,
                        sequence,
                        kSourceSystem,
                        kSourceComponent);
                    std::cout << std::setprecision(17)
                              << "FRAME seq=" << static_cast<unsigned int>(sequence)
                              << " reset=" << static_cast<unsigned int>(result.estimate.reset_counter)
                              << " x=" << result.estimate.x_m
                              << " y=" << result.estimate.y_m
                              << " z=" << result.estimate.z_m
                              << " yaw=" << result.estimate.yaw_rad
                              << " hex=" << to_hex(frame) << '\n';
                    sequence = static_cast<std::uint8_t>(sequence + 1U);
                }
            } else if (command == "RESET") {
                estimator.reset_tracking();
                std::cout << "RESET reset=" << static_cast<unsigned int>(estimator.reset_counter()) << '\n';
            } else if (command == "CLEAR") {
                estimator.clear_start_altitude();
                std::cout << "CLEARED reset=" << static_cast<unsigned int>(estimator.reset_counter()) << '\n';
            } else if (command == "STATUS") {
                std::cout << "STATUS initialized=" << (estimator.start_altitude_initialized() ? 1 : 0)
                          << " reset_required=" << (estimator.reset_required() ? 1 : 0)
                          << " reset=" << static_cast<unsigned int>(estimator.reset_counter()) << '\n';
            } else if (command == "QUIT") {
                std::cout << "BYE" << '\n' << std::flush;
                return 0;
            } else if (!command.empty()) {
                throw std::invalid_argument("unknown command");
            }
        } catch (const std::exception& error) {
            std::cout << "ERROR message=" << error.what() << '\n';
        }
        std::cout << std::flush;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--self-test") {
        const auto passed = self_test();
        std::cout << "route_local_odometry_sitl_producer_self_test passed="
                  << (passed ? "true" : "false") << '\n';
        return passed ? 0 : 1;
    }
    if (argc != 1) {
        std::cerr << "Usage: route_local_odometry_sitl_producer [--self-test]" << '\n';
        return 2;
    }
    return interactive_main();
}
