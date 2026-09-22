#include <barrier>
#include <cassert>
#include <future>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "visual_homing/gray8_route_matcher.hpp"

namespace {
vh::RouteSignatureFile fixture() {
    vh::RouteSignatureFile route;
    for (const auto& pixels : std::vector<std::vector<std::uint8_t>>{
             {0, 0, 0, 0}, {10, 20, 30, 40}, {10, 20, 30, 40}, {40, 30, 20, 10}}) {
        vh::RouteSignatureEntry entry;
        entry.width = entry.height = 2;
        entry.payload = pixels;
        route.entries.push_back(entry);
    }
    return route;
}

vh::Frame query() {
    return {.id = 7, .width = 2, .height = 2, .data = {10, 20, 30, 40}};
}

void check_diagnostics(const vh::Gray8RouteMatcher& matcher) {
    // Hand-calculated edge maps are [0,0,0,0] and [30,20,10,0].
    // Main match selects the first tie; diagnostic top-N inserts the last tie first.
    const auto result = matcher.probe_edge_diagnostics(query(), 4);
    assert(result.top_candidates.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        const auto& candidate = result.top_candidates[i];
        assert(candidate.route_index == 3 - i);
        assert(candidate.progress == static_cast<double>(3 - i) / 3.0);
        assert(candidate.confidence == (i < 3 ? 1.0 : 1.0 - 60.0 / 1020.0));
    }
    assert(result.zone_candidates.size() == 5);
    const std::vector<const char*> names{"start", "early", "mid", "late", "end"};
    const std::vector<std::size_t> indices{0, 1, 0, 2, 3};
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& zone = result.zone_candidates[i];
        assert(std::string(zone.name) == names[i]);
        assert(zone.valid == (i != 2));
        assert(zone.candidate.route_index == indices[i]);
    }
    const auto zones_only = matcher.probe_edge_diagnostics(query(), 0);
    assert(zones_only.top_candidates.empty());
    assert(zones_only.zone_candidates[4].candidate.route_index == 3);
}

void expect_bad_route(vh::RouteSignatureFile route, const std::string& message) {
    bool rejected = false;
    try {
        // Rejection must happen during construction, before any match or probe.
        vh::Gray8RouteMatcher matcher(std::move(route), {});
    } catch (const std::runtime_error& error) {
        rejected = true;
        assert(error.what() == message);
    }
    assert(rejected);
}
} // namespace

int main() {
    const auto route = fixture();
    for (const int invalid_kind : {0, 1, 2, 3, 4}) {
        auto malformed = route;
        auto& entry = malformed.entries.back();
        if (invalid_kind == 0) entry.width = 0;
        if (invalid_kind == 1) entry.height = 0;
        if (invalid_kind == 2) entry.payload.pop_back();
        if (invalid_kind == 3) entry.payload.push_back(0);
        if (invalid_kind == 4) entry.format = vh::PixelFormat::Bgr8;
        expect_bad_route(std::move(malformed), invalid_kind == 4
            ? "Gray8 route matcher only accepts Gray8 route entries"
            : "Gray8 edge diagnostics received malformed payload");
    }

    vh::Gray8RouteMatcher matcher(route, {.window_radius = 1, .top_candidate_count = 4});
    auto copy_before = matcher;
    const auto before = matcher.match(query());
    auto malformed_frame = query();
    malformed_frame.data.pop_back();
    bool rejected = false;
    try {
        (void)matcher.probe_edge_diagnostics(malformed_frame, 4);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
    check_diagnostics(matcher);
    check_diagnostics(matcher);
    check_diagnostics(copy_before);
    const auto after = matcher.match(query());
    assert(before.route_index == after.route_index && before.confidence == after.confidence);
    auto copy_after = matcher;
    auto moved = std::move(copy_after);
    check_diagnostics(moved);
    auto other_route = route;
    other_route.entries.resize(1);
    vh::Gray8RouteMatcher assigned(other_route, {});
    (void)assigned.probe_edge_diagnostics(query(), 1);
    assigned = matcher;
    check_diagnostics(assigned);

    // Simultaneous first const probes were read-only on the eager implementation.
    const vh::Gray8RouteMatcher shared(route, {});
    std::barrier ready(8);
    std::vector<std::future<void>> workers;
    for (int i = 0; i < 8; ++i) {
        workers.push_back(std::async(std::launch::async, [&] {
            ready.arrive_and_wait();
            for (int repeat = 0; repeat < 10; ++repeat) check_diagnostics(shared);
        }));
    }
    for (auto& worker : workers) worker.get();
}
