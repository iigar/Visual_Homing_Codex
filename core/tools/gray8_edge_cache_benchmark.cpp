// Linux/WSL one-process probe. Run repeatedly in fresh processes to avoid allocator reuse.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "visual_homing/gray8_route_matcher.hpp"
#include "visual_homing/route_signature.hpp"

namespace {
using Timer = std::chrono::steady_clock;

long rss_kib() {
    // smaps_rollup reports current resident pages, not the process high-water mark.
    std::ifstream input("/proc/self/smaps_rollup");
    std::string line;
    while (std::getline(input, line)) {
        if (line.starts_with("Rss:")) {
            long value = 0;
            std::istringstream fields(line.substr(4));
            if (fields >> value) return value;
        }
    }
    throw std::runtime_error("RSS probe requires readable /proc/self/smaps_rollup (Linux/WSL)");
}

double micros(Timer::time_point start, Timer::time_point end) {
    return std::chrono::duration<double, std::micro>(end - start).count();
}

void print_candidates(const vh::RouteMatchEdgeDiagnostics& diagnostics) {
    std::cout << "{\"top\":[";
    bool separator = false;
    for (const auto& c : diagnostics.top_candidates) {
        if (separator) std::cout << ',';
        separator = true;
        std::cout << '[' << c.route_index << ',' << c.progress << ',' << c.confidence << ']';
    }
    std::cout << "],\"zones\":[";
    separator = false;
    for (const auto& z : diagnostics.zone_candidates) {
        if (separator) std::cout << ',';
        separator = true;
        std::cout << "[\"" << z.name << "\"," << z.start_progress << ',' << z.end_progress
                  << ',' << (z.valid ? "true" : "false") << ',' << z.candidate.route_index
                  << ',' << z.candidate.progress << ',' << z.candidate.confidence << ']';
    }
    std::cout << "]}";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("usage: gray8_edge_cache_benchmark ROUTE.vhrs");
        auto route = vh::read_route_signature_file(argv[1]);
        if (route.entries.empty()) throw std::invalid_argument("Empty route");
        const auto entries = route.entries.size();
        const auto& entry = route.entries[entries / 2];
        const vh::Frame frame{.id = entry.frame_id, .width = entry.width,
            .height = entry.height, .format = entry.format, .data = entry.payload};
        const auto rss_loaded = rss_kib();
        const auto init_start = Timer::now();
        vh::Gray8RouteMatcher matcher(std::move(route), {.window_radius = 30});
        const auto init_end = Timer::now();
        const auto rss_initialized = rss_kib();
        const auto match = matcher.match(frame);
        const auto rss_matched = rss_kib();
        const auto first_start = Timer::now();
        const auto first = matcher.probe_edge_diagnostics(frame, 5);
        const auto first_end = Timer::now();
        const auto rss_first = rss_kib();
        const auto repeat_start = Timer::now();
        const auto repeat = matcher.probe_edge_diagnostics(frame, 5);
        const auto repeat_end = Timer::now();
        const auto rss_repeat = rss_kib();
        std::cout << std::setprecision(17)
                  << "{\"entries\":" << entries << ",\"init_us\":" << micros(init_start, init_end)
                  << ",\"first_edge_us\":" << micros(first_start, first_end)
                  << ",\"repeat_edge_us\":" << micros(repeat_start, repeat_end)
                  << ",\"rss_kib\":{\"loaded\":" << rss_loaded << ",\"initialized\":" << rss_initialized
                  << ",\"matched\":" << rss_matched << ",\"first_edge\":" << rss_first
                  << ",\"repeat_edge\":" << rss_repeat << "},\"match\":[" << match.route_index
                  << ',' << match.progress << ',' << match.confidence << "],\"first\":";
        print_candidates(first);
        std::cout << ",\"repeat\":";
        print_candidates(repeat);
        std::cout << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
