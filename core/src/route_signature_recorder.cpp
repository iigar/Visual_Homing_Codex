#include "visual_homing/route_signature_recorder.hpp"

#include <utility>

#include "visual_homing/route_signature_entry.hpp"

namespace vh {

void RouteSignatureRecorder::observe(const Frame& frame, const NavigationEstimate& nav) {
    route_.entries.push_back(make_route_signature_entry(frame, nav));
}

const RouteSignatureFile& RouteSignatureRecorder::route() const {
    return route_;
}

void RouteSignatureRecorder::write_to(const std::filesystem::path& path) const {
    write_route_signature_file(path, route_);
}

} // namespace vh
