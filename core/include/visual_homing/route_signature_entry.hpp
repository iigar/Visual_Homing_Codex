#pragma once

#include "visual_homing/frame.hpp"
#include "visual_homing/navigation.hpp"
#include "visual_homing/route_signature.hpp"

namespace vh {

RouteSignatureEntry make_route_signature_entry(const Frame& frame, const NavigationEstimate& nav);

} // namespace vh
