#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "visual_homing/route_artifact_check.hpp"

namespace {

vh::RouteSignatureEntry entry(std::uint64_t id, std::vector<std::uint8_t> payload) {
    vh::RouteSignatureEntry result;
    result.frame_id = id;
    result.timestamp_ns = 1000 + id;
    result.width = 2;
    result.height = 2;
    result.format = vh::PixelFormat::Gray8;
    result.payload = std::move(payload);
    return result;
}

} // namespace

int main() {
    vh::RouteSignatureFile route;
    route.entries.push_back(entry(0, {0, 10, 20, 30}));
    route.entries.push_back(entry(1, {40, 50, 60, 70}));
    route.entries.push_back(entry(2, {80, 90, 100, 110}));

    const auto summary = vh::self_match_route_signature(route);
    assert(summary.entries_checked == 3);
    assert(summary.valid_matches == 3);
    assert(summary.exact_index_matches == 3);
    assert(summary.minimum_confidence_seen > 0.999);
    assert(summary.average_confidence > 0.999);
    assert(summary.last_progress > 0.999);
    assert(summary.progress_monotonic);
    assert(summary.passed);

    const auto path = std::filesystem::temp_directory_path() / "visual_homing_route_artifact_check_test.vhrs";
    vh::write_route_signature_file(path, route);
    const auto file_summary = vh::self_match_route_signature_file(path);
    assert(file_summary.passed);
    assert(file_summary.entries_checked == 3);

    vh::RouteSignatureFile repetitive_route;
    repetitive_route.entries.push_back(entry(0, {20, 20, 20, 20}));
    repetitive_route.entries.push_back(entry(1, {20, 20, 20, 20}));
    repetitive_route.entries.push_back(entry(2, {21, 20, 20, 20}));
    const auto repetitive_summary = vh::self_match_route_signature(repetitive_route);
    assert(repetitive_summary.passed);
    assert(repetitive_summary.valid_matches == 3);
    assert(repetitive_summary.exact_index_matches < 3);
    assert(repetitive_summary.minimum_confidence_seen > 0.999);

    const auto repetitive_distinctiveness = vh::analyze_route_distinctiveness(repetitive_route);
    assert(repetitive_distinctiveness.entries_checked == 3);
    assert(repetitive_distinctiveness.adjacent_pairs_checked == 2);
    assert(repetitive_distinctiveness.low_texture_entries == 3);
    assert(repetitive_distinctiveness.exact_duplicate_entries == 2);
    assert(repetitive_distinctiveness.ambiguous_nearest_entries == 3);
    assert(repetitive_distinctiveness.low_texture_frame_ids.size() == 3);
    assert(repetitive_distinctiveness.low_texture_frame_ids[0] == 0);
    assert(repetitive_distinctiveness.exact_duplicate_frame_ids.size() == 2);
    assert(repetitive_distinctiveness.exact_duplicate_frame_ids[0] == 0);
    assert(repetitive_distinctiveness.ambiguous_nearest_frame_ids.size() == 3);
    assert(repetitive_distinctiveness.ambiguous_nearest_frame_ids[2] == 2);
    assert(repetitive_distinctiveness.minimum_payload_range == 0.0);
    assert(repetitive_distinctiveness.minimum_adjacent_mean_abs_diff == 0.0);
    assert(repetitive_distinctiveness.minimum_nearest_mean_abs_diff == 0.0);
    assert(repetitive_distinctiveness.warning);
    assert(!repetitive_distinctiveness.quality_pass);

    vh::RouteSignatureFile textured_route;
    vh::RouteSignatureEntry textured;
    textured.frame_id = 10;
    textured.timestamp_ns = 2000;
    textured.width = 4;
    textured.height = 2;
    textured.format = vh::PixelFormat::Gray8;
    textured.payload = {
        10, 40, 90, 160,
        10, 40, 90, 160,
    };
    textured_route.entries.push_back(textured);

    const auto perturbation = vh::perturbation_check_route_signature(textured_route, {
        .minimum_confidence = 0.85,
        .brightness_delta = 8,
        .noise_delta = 3,
        .shift_px = 1,
        .max_direction_shift_px = 2,
        .radians_per_pixel = 0.05,
    });
    assert(perturbation.entries_checked == 1);
    assert(perturbation.brightness_valid_matches == 1);
    assert(perturbation.noise_valid_matches == 1);
    assert(perturbation.shift_valid_matches == 1);
    assert(perturbation.shift_direction_matches == 1);
    assert(perturbation.minimum_brightness_confidence > 0.96);
    assert(perturbation.minimum_noise_confidence > 0.98);
    assert(perturbation.minimum_shift_confidence > 0.85);
    assert(perturbation.malformed_rejected);
    assert(perturbation.passed);

    const auto perturbation_path = std::filesystem::temp_directory_path() / "visual_homing_route_artifact_perturbation_test.vhrs";
    vh::write_route_signature_file(perturbation_path, textured_route);
    const auto file_perturbation = vh::perturbation_check_route_signature_file(
        perturbation_path,
        {.minimum_confidence = 0.85});
    assert(file_perturbation.passed);

    const auto textured_distinctiveness = vh::analyze_route_distinctiveness(textured_route);
    assert(textured_distinctiveness.entries_checked == 1);
    assert(textured_distinctiveness.adjacent_pairs_checked == 0);
    assert(textured_distinctiveness.low_texture_entries == 0);
    assert(textured_distinctiveness.exact_duplicate_entries == 0);
    assert(textured_distinctiveness.ambiguous_nearest_entries == 0);
    assert(textured_distinctiveness.low_texture_frame_ids.empty());
    assert(textured_distinctiveness.exact_duplicate_frame_ids.empty());
    assert(textured_distinctiveness.ambiguous_nearest_frame_ids.empty());
    assert(textured_distinctiveness.minimum_payload_range == 150.0);
    assert(!textured_distinctiveness.warning);
    assert(!textured_distinctiveness.quality_pass);

    vh::RouteSignatureFile quality_route;
    quality_route.entries.push_back(entry(0, {0, 0, 10, 10}));
    quality_route.entries.push_back(entry(1, {40, 40, 50, 50}));
    quality_route.entries.push_back(entry(2, {90, 90, 100, 100}));
    quality_route.entries.push_back(entry(3, {150, 150, 160, 160}));
    const auto quality_summary = vh::analyze_route_distinctiveness(quality_route);
    assert(quality_summary.low_texture_entries == 0);
    assert(quality_summary.exact_duplicate_entries == 0);
    assert(quality_summary.ambiguous_nearest_entries == 0);
    assert(quality_summary.low_texture_fraction == 0.0);
    assert(quality_summary.ambiguous_nearest_fraction == 0.0);
    assert(quality_summary.average_nearest_mean_abs_diff >= 5.0);
    assert(!quality_summary.warning);
    assert(quality_summary.quality_pass);

    const auto repetitive_perturbation = vh::perturbation_check_route_signature(repetitive_route, {
        .minimum_confidence = 0.90,
        .brightness_delta = 8,
        .noise_delta = 3,
        .shift_px = 1,
        .max_direction_shift_px = 2,
        .radians_per_pixel = 0.05,
    });
    assert(repetitive_perturbation.passed);
    assert(repetitive_perturbation.brightness_valid_matches == 3);
    assert(repetitive_perturbation.noise_valid_matches == 3);
    assert(repetitive_perturbation.shift_valid_matches == 3);
    assert(repetitive_perturbation.malformed_rejected);

    bool rejected_empty = false;
    try {
        vh::RouteSignatureFile empty;
        (void)vh::self_match_route_signature(empty);
    } catch (const std::invalid_argument&) {
        rejected_empty = true;
    }
    assert(rejected_empty);

    bool rejected_empty_perturbation = false;
    try {
        vh::RouteSignatureFile empty;
        (void)vh::perturbation_check_route_signature(empty);
    } catch (const std::invalid_argument&) {
        rejected_empty_perturbation = true;
    }
    assert(rejected_empty_perturbation);

    return 0;
}
