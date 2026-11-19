#include "driving_session.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace pitwall::app {
namespace {
simulation::VehicleState spawn(const track::Track& track) {
    // Start behind the line so the first lap begins after a genuine forward crossing.
    const auto pose = track.pose_at(-2.0);
    simulation::VehicleState result;
    result.position_m = pose.position_m;
    result.heading_rad = std::atan2(pose.forward.y, pose.forward.x);
    return result;
}
} // namespace

DrivingSession::DrivingSession(track::Track track)
    : laps_(std::move(track)), simulation_({}, 8, spawn(laps_.track())) {
    (void)laps_.update(simulation_.state().position_m, std::chrono::microseconds{0});
}

simulation::FrameResult DrivingSession::advance(std::chrono::microseconds frame,
                                               simulation::ControlInput controls) {
    simulation::validate_controls(controls);
    if (frame.count() < 0 || frame > std::chrono::seconds{60}) {
        throw std::invalid_argument("Frame duration must be in [0,60s]");
    }
    if (paused_) { return {}; }
    const auto result = simulation_.advance(frame, controls);
    for (const auto& tick : simulation_.last_ticks()) {
        (void)laps_.update(tick.state.position_m, tick.time);
    }
    interpolation_alpha_ = result.interpolation_alpha;
    const auto maximum = std::chrono::microseconds::max();
    dropped_time_ = result.dropped_time > maximum - dropped_time_
                        ? maximum : dropped_time_ + result.dropped_time;
    return result;
}

void DrivingSession::restart() {
    if (session_id_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Session ID exhausted");
    }
    simulation_.reset();
    laps_.reset();
    (void)laps_.update(simulation_.state().position_m, std::chrono::microseconds{0});
    ++session_id_;
    paused_ = false;
    interpolation_alpha_ = 0.0;
    dropped_time_ = {};
}

simulation::VehicleState DrivingSession::render_state() const noexcept {
    auto result = simulation_.state();
    if (!paused_) {
        const auto& previous = simulation_.previous_state();
        const double alpha = interpolation_alpha_;
        result.position_m.x = previous.position_m.x + alpha * (result.position_m.x - previous.position_m.x);
        result.position_m.y = previous.position_m.y + alpha * (result.position_m.y - previous.position_m.y);
        const double delta = std::remainder(result.heading_rad - previous.heading_rad,
                                             2.0 * std::numbers::pi);
        result.heading_rad = previous.heading_rad + alpha * delta;
    }
    return result;
}

} // namespace pitwall::app
