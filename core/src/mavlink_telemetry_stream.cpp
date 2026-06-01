#include "visual_homing/mavlink_telemetry_stream.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

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

} // namespace

MavlinkTelemetryStream::MavlinkTelemetryStream(MavlinkTelemetryStreamConfig config)
    : config_(std::move(config)) {
    if (config_.device_path.empty()) {
        throw std::invalid_argument("MAVLink telemetry stream device path must not be empty");
    }
    if (config_.baud_rate <= 0) {
        throw std::invalid_argument("MAVLink telemetry stream baud rate must be positive");
    }
}

MavlinkTelemetryStream::~MavlinkTelemetryStream() {
    stop();
}

bool MavlinkTelemetryStream::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return true;
    }

    stop_requested_ = false;
    bytes_captured_ = 0;
    bytes_.clear();
    last_error_.clear();
    worker_ = std::thread(&MavlinkTelemetryStream::read_loop, this);
    return true;
}

void MavlinkTelemetryStream::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

MavlinkTelemetryStreamSnapshot MavlinkTelemetryStream::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    MavlinkTelemetryStreamSnapshot snapshot;
    snapshot.supported = supported_;
    snapshot.opened = opened_;
    snapshot.running = running_;
    snapshot.bytes_captured = bytes_captured_;
    snapshot.inspection = inspect_mavlink_telemetry_bytes(bytes_);
    return snapshot;
}

const std::string& MavlinkTelemetryStream::last_error() const {
    return last_error_;
}

void MavlinkTelemetryStream::read_loop() {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_ = "Read-only MAVLink telemetry stream is only supported on POSIX serial devices";
    supported_ = false;
    opened_ = false;
    running_ = false;
#else
    try {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            supported_ = true;
        }

        FileDescriptor fd(open(config_.device_path.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK));
        if (fd.get() < 0) {
            throw std::runtime_error(errno_message("Could not open MAVLink telemetry device for read: " + config_.device_path));
        }
        configure_serial_read_only(fd.get(), config_.baud_rate);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            opened_ = true;
            running_ = true;
        }

        char buffer[512]{};
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_requested_) {
                    break;
                }
            }

            const auto bytes_read = read(fd.get(), buffer, sizeof(buffer));
            if (bytes_read > 0) {
                std::lock_guard<std::mutex> lock(mutex_);
                bytes_.append(buffer, static_cast<std::size_t>(bytes_read));
                bytes_captured_ += static_cast<std::uint64_t>(bytes_read);
                continue;
            }
            if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                throw std::runtime_error(errno_message("Could not read MAVLink telemetry device"));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } catch (const std::exception& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_ = error.what();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
#endif
}

} // namespace vh
