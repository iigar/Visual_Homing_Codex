#include <cassert>
#include <cmath>
#include <filesystem>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "visual_homing/camera_smoke.hpp"

namespace {

vh::LiveRouteMatchingConfig config_without_route() {
    vh::LiveRouteMatchingConfig config;
    config.route_path = std::filesystem::temp_directory_path()
        / "visual_homing_config_test_missing_route" / "absent.vhrs";
    assert(!std::filesystem::exists(config.route_path));
    return config;
}

void expect_rejected(const vh::LiveRouteMatchingConfig& config, const std::string& field) {
    std::ostringstream metrics;
    bool rejected = false;
    try {
        (void)vh::match_live_camera_route(config, metrics);
    } catch (const std::invalid_argument& error) {
        rejected = true;
        assert(std::string(error.what()).find(field) != std::string::npos);
    }
    assert(rejected);
    assert(metrics.str().empty());
}

void expect_valid_config(const vh::LiveRouteMatchingConfig& config) {
    std::ostringstream metrics;
    bool reached_route_read = false;
    try {
        (void)vh::match_live_camera_route(config, metrics);
    } catch (const std::runtime_error& error) {
        reached_route_read = std::string(error.what()).find(
            "Could not open route signature file for read:") != std::string::npos;
    }
    assert(reached_route_read);
    assert(metrics.str().empty());
}

} // namespace

int main() {
    // Direct library callers must receive the same validation as CLI callers.
    struct Field {
        double vh::LiveRouteMatchingConfig::* member;
        const char* name;
    };
    for (const auto& field : {
             Field{&vh::LiveRouteMatchingConfig::endpoint_start_progress, "endpoint_start_progress"},
             Field{&vh::LiveRouteMatchingConfig::endpoint_end_progress, "endpoint_end_progress"},
             Field{&vh::LiveRouteMatchingConfig::max_progress_rollback, "max_progress_rollback"}}) {
        for (const auto value : {std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity(), -1.0}) {
            auto config = config_without_route();
            config.*(field.member) = value;
            expect_rejected(config, field.name);
        }
    }
    auto config = config_without_route();
    expect_valid_config(config);
    config.endpoint_start_progress = 0.0;
    config.endpoint_end_progress = 1.0;
    config.max_progress_rollback = 0.0;
    expect_valid_config(config);
    config.max_progress_rollback = std::numeric_limits<double>::max();
    expect_valid_config(config);
    config.endpoint_start_progress = std::nextafter(0.0, -1.0);
    expect_rejected(config, "endpoint_start_progress");
    config.endpoint_start_progress = 0.0;
    config.endpoint_end_progress = std::nextafter(1.0, 2.0);
    expect_rejected(config, "endpoint_end_progress");
    config.endpoint_start_progress = config.endpoint_end_progress = 0.5;
    expect_rejected(config, "endpoint_start_progress must be less than endpoint_end_progress");
}
