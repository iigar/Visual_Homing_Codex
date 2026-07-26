#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include "visual_homing/live_route_verification.hpp"

namespace {

using namespace std::chrono_literals;

struct CapturedContext {
    std::mutex mutex;
    std::uint64_t calls = 0;
    vh::Frame frame;
    vh::LiveVerificationFrameContext context;
    bool fail = false;
    bool block = false;
    bool entered = false;
    bool released = false;
    std::condition_variable condition;
};

class CapturingProcessor final : public vh::VerificationPublicationProcessor {
public:
    explicit CapturingProcessor(std::shared_ptr<CapturedContext> captured)
        : captured_(std::move(captured)) {}

    vh::LiveVerificationFrameResult process(
        const vh::Frame& native_frame,
        const vh::LiveVerificationFrameContext& context) override {
        std::unique_lock lock(captured_->mutex);
        captured_->entered = true;
        captured_->condition.notify_all();
        captured_->condition.wait(lock, [this]() {
            return !captured_->block || captured_->released;
        });
        if (captured_->fail) {
            throw std::runtime_error("capture_failed");
        }
        ++captured_->calls;
        captured_->frame = native_frame;
        captured_->context = context;
        vh::LiveVerificationFrameResult result;
        result.decision.valid = true;
        return result;
    }

private:
    std::shared_ptr<CapturedContext> captured_;
};

vh::Timestamp timestamp_ms(std::int64_t value) {
    return vh::Timestamp(std::chrono::milliseconds(value));
}

vh::Frame native_frame() {
    vh::Frame result;
    result.id = 42;
    result.timestamp = timestamp_ms(1000);
    result.width = 2;
    result.height = 2;
    result.format = vh::PixelFormat::Gray8;
    result.data = {1, 2, 3, 4};
    return result;
}

vh::LiveRouteVerificationObservation observation() {
    vh::LiveRouteVerificationObservation result;
    result.match.timestamp = timestamp_ms(1000);
    result.match.route_index = 7;
    result.match.progress = 0.4;
    result.match.direction_error_rad = 0.02;
    result.match.direction_observation_valid = true;
    result.match.confidence = 0.92;
    result.match.valid = true;
    result.tracked_route_progress = 0.38;
    result.health.state = vh::HealthState::Ready;
    result.health.timestamp = timestamp_ms(1020);
    result.health.frames_seen = 1;
    result.health.route_match_confidence = 0.92;
    result.health.camera_ok = true;
    result.health.mavlink_ok = true;
    result.health.navigation_ok = true;
    result.altitude = {true, timestamp_ms(1010), 0.52};
    result.scale_ratio = {true, timestamp_ms(1010), 1.03};
    result.yaw = {true, timestamp_ms(1010), 0.08};
    result.publication_metadata.route_segment_id = 3;
    result.publication_metadata.allowed_directions = vh::route_gate_direction_forward;
    result.publication_metadata.minimum_altitude_m = 0.4;
    result.publication_metadata.maximum_altitude_m = 0.8;
    result.publication_metadata.minimum_scale_ratio = 0.8;
    result.publication_metadata.maximum_scale_ratio = 1.2;
    return result;
}

vh::LiveRouteVerificationLocalPoseObservation local_pose() {
    vh::LiveRouteVerificationLocalPoseObservation result;
    result.valid = true;
    result.frame_id = 42;
    result.timestamp = timestamp_ms(1015);
    result.frame_id_name = "yard-origin";
    result.frame_revision = "2026-07-27-a";
    result.frame_convention = "LOCAL_NED";
    result.x_m = 3.8;
    result.y_m = 0.1;
    result.z_m = -0.02;
    result.yaw_rad = 0.08;
    result.position_uncertainty_m = 0.2;
    result.approach_radius_m = 1.0;
    return result;
}

vh::LiveRouteVerificationProducerConfig producer_config(bool local_pose_enabled = false) {
    vh::LiveRouteVerificationProducerConfig result;
    result.minimum_match_confidence = 0.8;
    result.maximum_frame_context_age_ms = 100.0;
    result.maximum_scalar_age_ms = 50.0;
    result.maximum_local_pose_age_ms = 50.0;
    result.require_mavlink_health = true;
    if (local_pose_enabled) {
        result.local_frame_id = "yard-origin";
        result.local_frame_revision = "2026-07-27-a";
        result.local_frame_convention = "LOCAL_NED";
    }
    return result;
}

vh::BoundedVerificationPublisher make_publisher(
    const std::shared_ptr<CapturedContext>& captured,
    std::size_t capacity = 2) {
    vh::BoundedVerificationPublisherConfig config;
    config.queue_capacity = capacity;
    return vh::BoundedVerificationPublisher(
        std::move(config),
        std::make_unique<CapturingProcessor>(captured));
}

void expect_rejected(
    vh::LiveRouteVerificationProducer& producer,
    const vh::Frame& frame,
    const vh::LiveRouteVerificationObservation& value,
    const char* reason) {
    const auto result = producer.submit(frame, value);
    assert(result.status == vh::LiveRouteVerificationStatus::Rejected);
    assert(result.reason == reason);
    assert(!result.context);
    assert(!result.submission);
}

} // namespace

int main() {
    {
        auto captured = std::make_shared<CapturedContext>();
        auto publisher = make_publisher(captured);
        bool rejected = false;
        try {
            auto invalid = producer_config();
            invalid.minimum_match_confidence = 1.1;
            vh::LiveRouteVerificationProducer producer(invalid, publisher);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);

        rejected = false;
        try {
            auto invalid = producer_config();
            invalid.local_frame_id = "partial";
            vh::LiveRouteVerificationProducer producer(invalid, publisher);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto captured = std::make_shared<CapturedContext>();
        auto publisher = make_publisher(captured);
        vh::LiveRouteVerificationProducer producer(producer_config(), publisher);
        const auto before_start = producer.submit(native_frame(), observation());
        assert(before_start.status == vh::LiveRouteVerificationStatus::NotRunning);
        assert(before_start.submission);
        assert(before_start.submission->status == vh::VerificationSubmissionStatus::NotRunning);

        assert(publisher.start());
        const auto accepted = producer.submit(native_frame(), observation());
        assert(accepted.status == vh::LiveRouteVerificationStatus::Accepted);
        assert(accepted.context);
        assert(!accepted.context->has_local_pose);
        assert(accepted.context->route_progress == 0.38);
        assert(accepted.context->altitude_m == 0.52);
        assert(accepted.context->scale_ratio == 1.03);
        assert(accepted.context->yaw_rad == 0.08);
        assert(accepted.context->publication_metadata.route_segment_id == 3);
        assert(publisher.wait_until_idle(1000ms));
        publisher.stop(true);

        {
            std::lock_guard lock(captured->mutex);
            assert(captured->calls == 1);
            assert(captured->frame.id == 42);
            assert(captured->context.health_ready);
            assert(!captured->context.has_local_pose);
        }
        const auto metrics = producer.metrics();
        assert(metrics.observations == 2);
        assert(metrics.progress_only_contexts == 2);
        assert(metrics.not_running == 1);
        assert(metrics.accepted == 1);
        assert(metrics.rejected == 0);
        assert(std::string(vh::live_route_verification_status_name(
            vh::LiveRouteVerificationStatus::Backpressure)) == "backpressure");
    }
    {
        auto captured = std::make_shared<CapturedContext>();
        auto publisher = make_publisher(captured);
        vh::LiveRouteVerificationProducer producer(producer_config(true), publisher);
        assert(publisher.start());
        auto value = observation();
        value.local_pose = local_pose();
        const auto accepted = producer.submit(native_frame(), value);
        assert(accepted.status == vh::LiveRouteVerificationStatus::Accepted);
        assert(accepted.context && accepted.context->has_local_pose);
        assert(accepted.context->local_x_m == 3.8);
        assert(accepted.context->local_position_uncertainty_m == 0.2);
        assert(publisher.wait_until_idle(1000ms));
        publisher.stop(true);
        assert(producer.metrics().local_pose_contexts == 1);
    }
    {
        auto captured = std::make_shared<CapturedContext>();
        auto publisher = make_publisher(captured);
        vh::LiveRouteVerificationProducer producer(producer_config(), publisher);
        auto value = observation();
        value.match.valid = false;
        expect_rejected(producer, native_frame(), value, "route_match_invalid");

        value = observation();
        value.match.timestamp = timestamp_ms(999);
        expect_rejected(producer, native_frame(), value, "route_match_frame_mismatch");

        value = observation();
        value.match.confidence = 0.79;
        expect_rejected(producer, native_frame(), value, "route_match_confidence_low");

        value = observation();
        value.tracked_route_progress.reset();
        expect_rejected(producer, native_frame(), value, "tracked_route_progress_invalid");

        value = observation();
        value.health.timestamp = timestamp_ms(1200);
        expect_rejected(producer, native_frame(), value, "health_frame_context_stale");

        value = observation();
        value.health.mavlink_ok = false;
        expect_rejected(producer, native_frame(), value, "health_not_ready");

        value = observation();
        value.health.route_match_confidence = 0.91;
        expect_rejected(producer, native_frame(), value, "health_match_confidence_mismatch");

        value = observation();
        value.altitude.valid = false;
        expect_rejected(producer, native_frame(), value, "altitude_observation_invalid");

        value = observation();
        value.scale_ratio.value = 0.0;
        expect_rejected(producer, native_frame(), value, "scale_observation_invalid");

        value = observation();
        value.yaw.timestamp = timestamp_ms(900);
        expect_rejected(producer, native_frame(), value, "yaw_observation_invalid");

        value = observation();
        value.altitude.value = std::numeric_limits<double>::quiet_NaN();
        expect_rejected(producer, native_frame(), value, "altitude_observation_invalid");

        assert(producer.metrics().rejected == 11);
        assert(producer.metrics().last_rejection_reason == "altitude_observation_invalid");
        assert(publisher.metrics().submissions == 0);
    }
    {
        auto captured = std::make_shared<CapturedContext>();
        auto publisher = make_publisher(captured);
        vh::LiveRouteVerificationProducer no_frame_contract(producer_config(), publisher);
        auto value = observation();
        value.local_pose = local_pose();
        expect_rejected(
            no_frame_contract,
            native_frame(),
            value,
            "local_pose_frame_not_configured");

        vh::LiveRouteVerificationProducer producer(producer_config(true), publisher);
        value = observation();
        value.local_pose = local_pose();
        value.local_pose->valid = false;
        expect_rejected(producer, native_frame(), value, "local_pose_invalid");

        value = observation();
        value.local_pose = local_pose();
        value.local_pose->frame_id = 41;
        expect_rejected(producer, native_frame(), value, "local_pose_frame_mismatch");

        value = observation();
        value.local_pose = local_pose();
        value.local_pose->timestamp = timestamp_ms(900);
        expect_rejected(producer, native_frame(), value, "local_pose_stale");

        value = observation();
        value.local_pose = local_pose();
        value.local_pose->frame_revision = "other";
        expect_rejected(producer, native_frame(), value, "local_pose_coordinate_frame_mismatch");

        value = observation();
        value.local_pose = local_pose();
        value.local_pose->approach_radius_m = 0.1;
        expect_rejected(producer, native_frame(), value, "local_pose_quality_invalid");
    }
    {
        auto captured = std::make_shared<CapturedContext>();
        captured->block = true;
        auto publisher = make_publisher(captured, 1);
        vh::LiveRouteVerificationProducer producer(producer_config(), publisher);
        assert(publisher.start());
        const auto first = producer.submit(native_frame(), observation());
        assert(first.status == vh::LiveRouteVerificationStatus::Accepted);
        {
            std::unique_lock lock(captured->mutex);
            assert(captured->condition.wait_for(lock, 1000ms, [&captured]() {
                return captured->entered;
            }));
        }
        const auto second = producer.submit(native_frame(), observation());
        assert(second.status == vh::LiveRouteVerificationStatus::Backpressure);
        assert(second.reason == "verification_publisher_backpressure");
        {
            std::lock_guard lock(captured->mutex);
            captured->released = true;
        }
        captured->condition.notify_all();
        assert(publisher.wait_until_idle(1000ms));
        publisher.stop(true);
        assert(producer.metrics().accepted == 1);
        assert(producer.metrics().backpressure == 1);
    }
    {
        auto captured = std::make_shared<CapturedContext>();
        captured->fail = true;
        auto publisher = make_publisher(captured);
        vh::LiveRouteVerificationProducer producer(producer_config(), publisher);
        assert(publisher.start());
        assert(producer.submit(native_frame(), observation()).status
            == vh::LiveRouteVerificationStatus::Accepted);
        assert(publisher.wait_until_idle(1000ms));
        const auto failed = producer.submit(native_frame(), observation());
        assert(failed.status == vh::LiveRouteVerificationStatus::Failed);
        assert(failed.reason == "capture_failed");
        publisher.stop(true);
        assert(producer.metrics().failed == 1);
    }
    return 0;
}
