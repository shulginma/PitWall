#pragma once

#include "driving_session.hpp"

namespace pitwall::app {
void draw_simulator_ui(DrivingSession& session, simulation::ControlInput controls);
// smoke_path != nullptr: render hidden frames, save a BMP and exit.
int run_gui(const char* smoke_path = nullptr, bool native_smoke = false);
}
