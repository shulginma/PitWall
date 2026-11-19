#pragma once

#include <pitwall/simulation/fixed_step.hpp>
#include <pitwall/track/lap_tracker.hpp>

#include <chrono>
#include <cstdint>

namespace pitwall::app {

// App composition without any graphics headers. Every physics tick reaches lap timing.
class DrivingSession {
public:
    explicit DrivingSession(track::Track track = track::make_oval_track());
    [[nodiscard]] simulation::FrameResult advance(std::chrono::microseconds frame,
                                                  simulation::ControlInput controls);
    void set_paused(bool paused) noexcept { paused_ = paused; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    void restart();
    [[nodiscard]] const simulation::FixedStepSimulation& simulation() const noexcept { return simulation_; }
    [[nodiscard]] const track::LapTracker& laps() const noexcept { return laps_; }
    [[nodiscard]] std::uint64_t session_id() const noexcept { return session_id_; }
    [[nodiscard]] std::chrono::microseconds dropped_time() const noexcept { return dropped_time_; }
    [[nodiscard]] simulation::VehicleState render_state() const noexcept;

private:
    track::LapTracker laps_;
    simulation::FixedStepSimulation simulation_;
    std::uint64_t session_id_{1};
    bool paused_{};
    double interpolation_alpha_{};
    std::chrono::microseconds dropped_time_{};
};

} // namespace pitwall::app
