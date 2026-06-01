#include "visual_homing/mavlink_telemetry_capture.hpp"

#include "visual_homing/time.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

#if !defined(_WIN32)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace vh {
namespace {

#if !defined(_WIN32)
class FileDescriptor {
public:
    explicit FileDescriptor(int value)
        : value_(value) {}

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    ~FileDescriptor() {
        if (value_ >= 0) {
            close(value_);
        }
    }

    int get() const {
        return value_;
    }

private:
    int value_ = -1;
};

speed_t baud_to_speed(int baud_rate) {
    switch (baud_rate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 921600:
        return B921600;
    default:
        throw std::invalid_argument("Unsupported MAVLink telemetry baud rate: " + std::to_string(baud_rate));
    }
}

std::string errno_message(const std::string& prefix) {
    return prefix + ": " + std::strerror(errno);
}

void configure_serial_read_only(int fd, int baud_rate) {
    termios options{};
    if (tcgetattr(fd, &options) != 0) {
        throw std::runtime_error(errno_message("Could not read serial port attributes"));
    }

    cfmakeraw(&options);
    const auto speed = baud_to_speed(baud_rate);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    options.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
    options.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    options.c_cflag &= static_cast<tcflag_t>(~PARENB);
    options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    options.c_cflag |= CS8;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        throw std::runtime_error(errno_message("Could not configure serial port attributes"));
    }
}
#endif

void validate_capture_config(const MavlinkTelemetryCaptureConfig& config) {
    if (config.device_path.empty()) {
        throw std::invalid_argument("MAVLink telemetry device path must not be empty");
    }
    if (config.baud_rate <= 0) {
        throw std::invalid_argument("MAVLink telemetry baud rate must be positive");
    }
    if (config.duration_ms == 0) {
        throw std::invalid_argument("MAVLink telemetry capture duration must be positive");
    }
    if (config.output_path.empty()) {
        throw std::invalid_argument("MAVLink telemetry output path must not be empty");
    }
}

} // namespace

MavlinkTelemetryCaptureSummary capture_mavlink_telemetry_file(const MavlinkTelemetryCaptureConfig& config) {
    validate_capture_config(config);

    MavlinkTelemetryCaptureSummary summary;
    summary.output_path = config.output_path;

#if defined(_WIN32)
    (void)config;
    throw std::runtime_error("Read-only MAVLink telemetry capture is only supported on POSIX serial devices");
#else
    summary.supported = true;

    const auto parent = std::filesystem::path(config.output_path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    FileDescriptor fd(open(config.device_path.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK));
    if (fd.get() < 0) {
        throw std::runtime_error(errno_message("Could not open MAVLink telemetry device for read: " + config.device_path));
    }
    summary.opened = true;
    configure_serial_read_only(fd.get(), config.baud_rate);

    std::ofstream output(config.output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Could not open MAVLink telemetry output file for write: " + config.output_path);
    }

    const auto start = now();
    char buffer[512]{};
    while (milliseconds_between(start, now()) < static_cast<double>(config.duration_ms)) {
        const auto bytes_read = read(fd.get(), buffer, sizeof(buffer));
        if (bytes_read > 0) {
            output.write(buffer, bytes_read);
            summary.bytes_captured += static_cast<std::uint64_t>(bytes_read);
            continue;
        }
        if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            throw std::runtime_error(errno_message("Could not read MAVLink telemetry device"));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    output.flush();
    if (!output) {
        throw std::runtime_error("Could not write MAVLink telemetry output file: " + config.output_path);
    }

    summary.elapsed_ms = milliseconds_between(start, now());
    return summary;
#endif
}

} // namespace vh
