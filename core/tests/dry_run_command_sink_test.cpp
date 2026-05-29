#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

#include "visual_homing/dry_run_command_sink.hpp"

int main() {
    std::ostringstream output;
    vh::DryRunCommandSink sink(&output);

    bool rejected = false;
    try {
        vh::NavigationCommand command;
        sink.send(command);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    assert(sink.start());
    assert(sink.running());

    vh::NavigationCommand command;
    command.vx_mps = 3.0;
    command.yaw_rate_radps = 0.12;
    command.confidence = 0.88;
    command.valid = true;
    sink.send(command);

    assert(sink.commands().size() == 1);
    assert(sink.commands()[0].valid);
    assert(sink.commands()[0].vx_mps == 3.0);
    assert(output.str().find("dry_run_command valid=true") != std::string::npos);
    assert(output.str().find("yaw_rate_radps=0.12") != std::string::npos);

    sink.stop();
    assert(!sink.running());

    return 0;
}
