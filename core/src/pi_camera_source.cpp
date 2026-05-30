#include "visual_homing/pi_camera_source.hpp"

#include <stdexcept>

#ifdef VISUAL_HOMING_ENABLE_LIBCAMERA
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <libcamera/formats.h>
#include <libcamera/libcamera.h>
#endif

#include "visual_homing/time.hpp"

namespace vh {

#ifdef VISUAL_HOMING_ENABLE_LIBCAMERA

struct PiCameraSource::Backend {
    struct MappedPlane {
        void* address = MAP_FAILED;
        std::size_t length = 0;
    };

    explicit Backend(const PiCameraConfig& config)
        : config(config) {}

    ~Backend() {
        stop();
    }

    bool start(std::string& error) {
        manager = std::make_unique<libcamera::CameraManager>();
        if (manager->start() < 0) {
            error = "libcamera CameraManager failed to start";
            return false;
        }

        {
            auto cameras = manager->cameras();
            if (cameras.empty()) {
                error = "libcamera found no cameras";
                stop();
                return false;
            }
            camera = cameras.front();
            cameras.clear();
        }

        if (!camera) {
            error = "libcamera could not select first camera";
            stop();
            return false;
        }

        if (camera->acquire() < 0) {
            error = "libcamera camera acquire failed";
            stop();
            return false;
        }
        camera_acquired = true;

        camera_config = camera->generateConfiguration({libcamera::StreamRole::Viewfinder});
        if (!camera_config || camera_config->empty()) {
            error = "libcamera could not generate viewfinder configuration";
            stop();
            return false;
        }

        libcamera::StreamConfiguration& stream_config = camera_config->at(0);
        stream_config.size.width = static_cast<unsigned int>(config.width);
        stream_config.size.height = static_cast<unsigned int>(config.height);
        stream_config.pixelFormat = libcamera::formats::YUV420;

        const auto validation = camera_config->validate();
        if (validation == libcamera::CameraConfiguration::Invalid) {
            error = "libcamera rejected requested camera configuration";
            stop();
            return false;
        }

        actual_width = static_cast<int>(stream_config.size.width);
        actual_height = static_cast<int>(stream_config.size.height);

        if (camera->configure(camera_config.get()) < 0) {
            error = "libcamera camera configure failed";
            stop();
            return false;
        }

        stream = camera_config->at(0).stream();
        if (!stream) {
            error = "libcamera returned no stream after configure";
            stop();
            return false;
        }

        allocator = std::make_unique<libcamera::FrameBufferAllocator>(camera);
        if (allocator->allocate(stream) < 0) {
            error = "libcamera frame buffer allocation failed";
            stop();
            return false;
        }
        buffers_allocated = true;

        const auto& buffers = allocator->buffers(stream);
        if (buffers.empty()) {
            error = "libcamera allocated no frame buffers";
            stop();
            return false;
        }

        for (const auto& buffer : buffers) {
            std::vector<MappedPlane> planes;
            for (const libcamera::FrameBuffer::Plane& plane : buffer->planes()) {
                if (plane.fd.get() < 0 || plane.length == 0) {
                    error = "libcamera frame buffer plane is invalid";
                    stop();
                    return false;
                }

                void* memory = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
                if (memory == MAP_FAILED) {
                    error = std::string("mmap failed for libcamera frame buffer: ") + std::strerror(errno);
                    stop();
                    return false;
                }
                planes.push_back({memory, plane.length});
            }
            mapped_planes.emplace(buffer.get(), std::move(planes));

            std::unique_ptr<libcamera::Request> request = camera->createRequest();
            if (!request) {
                error = "libcamera request creation failed";
                stop();
                return false;
            }
            if (request->addBuffer(stream, buffer.get()) < 0) {
                error = "libcamera addBuffer failed";
                stop();
                return false;
            }
            requests.push_back(std::move(request));
        }

        camera->requestCompleted.connect(this, &Backend::on_request_completed);

        if (camera->start() < 0) {
            error = "libcamera camera start failed";
            stop();
            return false;
        }
        camera_started = true;
        active = true;

        for (std::unique_ptr<libcamera::Request>& request : requests) {
            if (camera->queueRequest(request.get()) < 0) {
                error = "libcamera queueRequest failed";
                stop();
                return false;
            }
        }

        return true;
    }

    void stop() {
        active = false;

        if (camera && camera_started) {
            camera->stop();
            camera_started = false;
        }

        if (camera) {
            camera->requestCompleted.disconnect(this);
        }

        requests.clear();

        for (auto& entry : mapped_planes) {
            for (MappedPlane& plane : entry.second) {
                if (plane.address != MAP_FAILED) {
                    munmap(plane.address, plane.length);
                    plane.address = MAP_FAILED;
                    plane.length = 0;
                }
            }
        }
        mapped_planes.clear();

        if (allocator && buffers_allocated && stream) {
            allocator->free(stream);
            buffers_allocated = false;
        }
        allocator.reset();

        if (camera && camera_acquired) {
            camera->release();
            camera_acquired = false;
        }
        camera.reset();
        camera_config.reset();
        stream = nullptr;

        if (manager) {
            manager->stop();
        }
        manager.reset();

        std::lock_guard<std::mutex> lock(frames_mutex);
        completed_frames.clear();
    }

    std::optional<Frame> poll() {
        std::lock_guard<std::mutex> lock(frames_mutex);
        if (completed_frames.empty()) {
            return std::nullopt;
        }

        Frame frame = std::move(completed_frames.front());
        completed_frames.pop_front();
        return frame;
    }

    void on_request_completed(libcamera::Request* request) {
        if (!active || request->status() == libcamera::Request::RequestCancelled) {
            return;
        }

        const auto buffers = request->buffers();
        const auto found = buffers.find(stream);
        if (found == buffers.end()) {
            requeue(request);
            return;
        }

        libcamera::FrameBuffer* buffer = found->second;
        const auto mapped = mapped_planes.find(buffer);
        if (mapped == mapped_planes.end() || mapped->second.empty()) {
            requeue(request);
            return;
        }

        const auto& plane = mapped->second.front();
        const auto& metadata = buffer->metadata();
        const std::size_t expected_bytes = static_cast<std::size_t>(actual_width) * static_cast<std::size_t>(actual_height);
        const std::size_t metadata_bytes = metadata.planes().empty()
            ? plane.length
            : static_cast<std::size_t>(metadata.planes().front().bytesused);
        const std::size_t bytes_to_copy = std::min({expected_bytes, metadata_bytes, plane.length});

        if (plane.address != MAP_FAILED && bytes_to_copy > 0) {
            Frame frame;
            frame.id = metadata.sequence;
            frame.timestamp = now();
            frame.width = actual_width;
            frame.height = actual_height;
            frame.format = PixelFormat::Gray8;
            frame.data.assign(
                static_cast<const std::uint8_t*>(plane.address),
                static_cast<const std::uint8_t*>(plane.address) + bytes_to_copy);
            frame.data.resize(expected_bytes, 0);

            std::lock_guard<std::mutex> lock(frames_mutex);
            if (completed_frames.size() >= max_completed_frames) {
                completed_frames.pop_front();
            }
            completed_frames.push_back(std::move(frame));
        }

        requeue(request);
    }

    void requeue(libcamera::Request* request) {
        if (!active || !camera) {
            return;
        }
        request->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(request);
    }

    PiCameraConfig config;
    std::unique_ptr<libcamera::CameraManager> manager;
    std::shared_ptr<libcamera::Camera> camera;
    std::unique_ptr<libcamera::CameraConfiguration> camera_config;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
    std::vector<std::unique_ptr<libcamera::Request>> requests;
    std::map<libcamera::FrameBuffer*, std::vector<MappedPlane>> mapped_planes;
    libcamera::Stream* stream = nullptr;
    int actual_width = 0;
    int actual_height = 0;
    bool camera_acquired = false;
    bool buffers_allocated = false;
    bool camera_started = false;
    bool active = false;
    std::mutex frames_mutex;
    std::deque<Frame> completed_frames;
    std::size_t max_completed_frames = 2;
};

#else

struct PiCameraSource::Backend {
    void stop() {}
};

#endif

PiCameraSource::PiCameraSource(PiCameraConfig config)
    : config_(config) {
    if (config_.width <= 0 || config_.height <= 0) {
        throw std::invalid_argument("PiCameraSource dimensions must be positive");
    }
    if (config_.frame_rate_hz <= 0) {
        throw std::invalid_argument("PiCameraSource frame_rate_hz must be positive");
    }
    if (config_.format != PixelFormat::Gray8) {
        throw std::invalid_argument("PiCameraSource initial backend only accepts Gray8 output");
    }
}

PiCameraSource::~PiCameraSource() = default;

bool PiCameraSource::start() {
    stop();
    last_error_.clear();

    if (!config_.enable_live_capture) {
        last_error_ = "PiCameraSource live capture is not enabled";
        running_ = false;
        return false;
    }

#ifdef VISUAL_HOMING_ENABLE_LIBCAMERA
    backend_ = std::make_unique<Backend>(config_);
    if (!backend_->start(last_error_)) {
        backend_.reset();
        running_ = false;
        if (last_error_.empty()) {
            last_error_ = "PiCameraSource libcamera backend failed to start";
        }
        return false;
    }
    running_ = true;
    return true;
#else
    last_error_ = "PiCameraSource libcamera backend is not compiled in";
    running_ = false;
    return false;
#endif
}

void PiCameraSource::stop() {
    if (backend_) {
        backend_->stop();
        backend_.reset();
    }
    running_ = false;
}

std::optional<Frame> PiCameraSource::poll() {
#ifdef VISUAL_HOMING_ENABLE_LIBCAMERA
    if (!backend_) {
        return std::nullopt;
    }
    return backend_->poll();
#else
    return std::nullopt;
#endif
}

bool PiCameraSource::running() const {
    return running_;
}

const PiCameraConfig& PiCameraSource::config() const {
    return config_;
}

const std::string& PiCameraSource::last_error() const {
    return last_error_;
}

} // namespace vh
