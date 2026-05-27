#pragma once

#include <filesystem>

#include "visual_homing/interfaces.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

class RouteSignatureRecorder final : public RouteRecorder {
public:
    void observe(const Frame& frame, const NavigationEstimate& nav) override;

    const RouteSignatureFile& route() const;
    void write_to(const std::filesystem::path& path) const;

private:
    RouteSignatureFile route_;
};

} // namespace vh
