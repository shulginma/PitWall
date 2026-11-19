#pragma once

#include "driving_session.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace session_tests {
using namespace std::chrono_literals;
using pitwall::app::DrivingSession;
using pitwall::simulation::FixedStepSimulation;
using pitwall::simulation::VehicleState;

inline void require(bool condition, std::string_view message) {
    if (!condition) { throw std::runtime_error(std::string{message}); }
}
template <typename F> void invalid(F function) {
    try { function(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid_argument");
}

inline void spawn() {
    DrivingSession session;
    const auto projection = session.laps().track().project(session.simulation().state().position_m);
    require(projection.on_track && !session.laps().status().running, "Spawn on road before timing");
    require(std::abs(projection.pose.progress_m - (session.laps().track().length_m() - 2.0)) < 1e-8,
            "Spawn two metres before start");
}

inline void tick_stream() {
    FixedStepSimulation simulation;
    (void)simulation.advance(25ms, {1.0, 0.0, 0.0});
    const auto ticks = simulation.last_ticks();
    require(ticks.size() == 2 && ticks[0].time == 10ms && ticks[1].time == 20ms,
            "Every executed tick has its simulation timestamp");
    require(ticks[1].state.position_m.x == simulation.state().position_m.x &&
                ticks[0].state.position_m.x == simulation.previous_state().position_m.x,
            "Final/previous states match tick stream");
    const double previous_x = simulation.previous_state().position_m.x;
    (void)simulation.advance(1ms, {});
    require(simulation.last_ticks().empty() && simulation.previous_state().position_m.x == previous_x,
            "Zero-tick frames clear stream without destroying interpolation state");
}

inline void spawn_reset() {
    VehicleState spawn_state;
    spawn_state.position_m = {100, 50};
    spawn_state.heading_rad = 1.0;
    FixedStepSimulation simulation{{}, 8, spawn_state};
    (void)simulation.advance(20ms, {1, 0, 0});
    simulation.reset();
    require(simulation.state().position_m.x == 100 && simulation.state().position_m.y == 50 &&
                simulation.state().heading_rad == 1.0 && simulation.tick_count() == 0,
            "Reset restores configured spawn, not world origin");
    spawn_state.position_m = {5, 6};
    simulation.reset(spawn_state);
    require(simulation.previous_state().position_m.x == 5 && simulation.last_ticks().empty(),
            "Changing spawn clears previous state and tick stream");
}

inline void bad_spawn() {
    FixedStepSimulation simulation;
    (void)simulation.advance(20ms, {1, 0, 0});
    const auto before = simulation.state();
    VehicleState bad;
    bad.heading_rad = std::numeric_limits<double>::quiet_NaN();
    invalid([&] { simulation.reset(bad); });
    require(simulation.tick_count() == 2 && simulation.state().speed_mps == before.speed_mps,
            "Bad spawn is rejected without modifying state");
    invalid([&] { (void)FixedStepSimulation{{}, 8, bad}; });
    simulation.reset();
    require(simulation.state().heading_rad == 0, "Rejected reset must not replace saved spawn");
}

inline void pause() {
    DrivingSession session;
    for (int i = 0; i < 120; ++i) { (void)session.advance(10ms, {1, 0, 0}); }
    const auto before = session.simulation().telemetry(1);
    const auto elapsed = session.laps().status().elapsed;
    session.set_paused(true);
    for (int i = 0; i < 100; ++i) { (void)session.advance(16ms, {1, 0, 1}); }
    require(session.simulation().simulation_time() == before.session_time &&
                session.simulation().state().position_m.x == before.position_m.x &&
                session.laps().status().elapsed == elapsed,
            "Pause freezes vehicle, simulation clock and lap clock");
    session.set_paused(false);
    (void)session.advance(10ms, {});
    require(session.simulation().simulation_time() == before.session_time + 10ms,
            "Resume advances from the frozen clock");
}

inline void restart() {
    DrivingSession session;
    const auto initial = session.simulation().state();
    (void)session.advance(1s, {1, 0, 0});
    session.set_paused(true);
    session.restart();
    require(session.session_id() == 2 && !session.paused() && session.dropped_time() == 0us &&
                session.simulation().tick_count() == 0 && session.laps().completed_laps().empty() &&
                !session.laps().status().running,
            "Restart resets all session components and advances identity");
    require(session.simulation().state().position_m.x == initial.position_m.x &&
                session.simulation().state().position_m.y == initial.position_m.y,
            "Restart uses the track spawn");
}

inline DrivingSession run_frames(std::int64_t fps) {
    DrivingSession session;
    for (std::int64_t i = 0; i < fps * 3; ++i) {
        const auto dt = std::chrono::microseconds{(i + 1) * 1'000'000 / fps - i * 1'000'000 / fps};
        (void)session.advance(dt, {0.4, 0, 0.1});
    }
    return session;
}

inline void frame_partition() {
    const auto a = run_frames(30);
    const auto b = run_frames(144);
    require(a.simulation().tick_count() == 300 && b.simulation().tick_count() == 300,
            "Session executes the same number of physical ticks");
    require(a.simulation().state().position_m.x == b.simulation().state().position_m.x &&
                a.simulation().state().position_m.y == b.simulation().state().position_m.y &&
                a.laps().status().elapsed == b.laps().status().elapsed &&
                a.laps().status().reason == b.laps().status().reason,
            "Physical and lap state are independent of render frame partitioning");
}

inline void controls() {
    DrivingSession session;
    for (int i = 0; i < 100; ++i) { (void)session.advance(10ms, {1, 0, 0}); }
    require(session.simulation().state().speed_mps > 5.0, "Gas accelerates");
    for (int i = 0; i < 100; ++i) { (void)session.advance(10ms, {0, 1, 0}); }
    require(session.simulation().state().speed_mps == 0.0, "Brake stops without reverse");
    require(session.laps().status().running, "Actual vehicle crossed start and began timing");
}

inline void invalid_frame() {
    DrivingSession session;
    session.set_paused(true);
    invalid([&] { (void)session.advance(-1us, {}); });
    invalid([&] { (void)session.advance(61s, {}); });
    invalid([&] { (void)session.advance(0us, {2, 0, 0}); });
    require(session.simulation().tick_count() == 0 && !session.laps().status().running,
            "Rejected paused input leaves session unchanged");
}

inline void interpolation() {
    DrivingSession session;
    const auto initial = session.simulation().state();
    (void)session.advance(15ms, {1, 0, 0});
    const auto actual = session.simulation().state();
    const auto render = session.render_state();
    const double full = std::hypot(actual.position_m.x - initial.position_m.x,
                                   actual.position_m.y - initial.position_m.y);
    const double half = std::hypot(render.position_m.x - initial.position_m.x,
                                   render.position_m.y - initial.position_m.y);
    require(std::abs(half - full * 0.5) < 1e-10, "Render halfway between ticks");
    require(session.simulation().state().position_m.x == actual.position_m.x,
            "Interpolation must not alter authoritative physics");
    session.set_paused(true);
    require(session.render_state().position_m.x == actual.position_m.x,
            "Paused rendering shows current physical pose");
}

struct Case { std::string_view name; void (*run)(); };
inline constexpr std::array cases{
    Case{"spawn", spawn}, Case{"tick_stream", tick_stream}, Case{"spawn_reset", spawn_reset},
    Case{"bad_spawn", bad_spawn}, Case{"pause", pause}, Case{"restart", restart},
    Case{"frame_partition", frame_partition}, Case{"controls", controls},
    Case{"invalid_frame", invalid_frame}, Case{"interpolation", interpolation}};
} // namespace session_tests
