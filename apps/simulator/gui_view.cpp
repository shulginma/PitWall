#include "gui.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace pitwall::app {
namespace {
constexpr ImU32 asphalt = IM_COL32(42, 49, 61, 255);
constexpr ImU32 accent = IM_COL32(142, 231, 157, 255);
constexpr ImU32 muted = IM_COL32(125, 145, 162, 255);
constexpr ImU32 white = IM_COL32(232, 240, 246, 255);

struct Camera {
    ImVec2 origin;
    float scale;
    domain::Vec2 center;
    ImVec2 point(domain::Vec2 p) const {
        return {origin.x + static_cast<float>(p.x - center.x) * scale,
                origin.y - static_cast<float>(p.y - center.y) * scale};
    }
};

Camera fit_camera(const track::Track& track, ImVec2 origin, ImVec2 size) {
    double min_x = std::numeric_limits<double>::max();
    double min_y = min_x;
    double max_x = -min_x;
    double max_y = -min_y;
    for (const auto p : track.centerline()) {
        min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
    }
    const double padding = track.road_width_m() + 12.0;
    const float scale = std::min(size.x / static_cast<float>(max_x - min_x + 2 * padding),
                                 size.y / static_cast<float>(max_y - min_y + 2 * padding));
    return {{origin.x + size.x * 0.5f, origin.y + size.y * 0.5f}, scale,
            {(min_x + max_x) * 0.5, (min_y + max_y) * 0.5}};
}

void road_stroke(ImDrawList* draw, const track::Track& track, const Camera& camera,
                 double width_m, ImU32 color) {
    const auto points = track.centerline();
    const float width = static_cast<float>(width_m) * camera.scale;
    for (std::size_t i = 0; i < points.size(); ++i) {
        draw->AddLine(camera.point(points[i]), camera.point(points[(i + 1) % points.size()]), color, width);
        draw->AddCircleFilled(camera.point(points[i]), width * 0.5f, color, 16);
    }
}

void gate(ImDrawList* draw, const track::Track& track, const Camera& camera,
           double progress, ImU32 color, bool finish) {
    const auto pose = track.pose_at(progress);
    const domain::Vec2 normal{-pose.forward.y, pose.forward.x};
    const auto world = [&](double along, double lateral) {
        return camera.point({pose.position_m.x + along * pose.forward.x + lateral * normal.x,
                             pose.position_m.y + along * pose.forward.y + lateral * normal.y});
    };
    const double half = track.road_width_m() * 0.5;
    if (finish) {
        for (int row = 0; row < 10; ++row) {
            const double a = -half + track.road_width_m() * static_cast<double>(row) / 10.0;
            const double b = a + track.road_width_m() / 10.0;
            for (int col = 0; col < 2; ++col) {
                const double x = static_cast<double>(col) * 0.6;
                draw->AddQuadFilled(world(x, a), world(x + 0.6, a), world(x + 0.6, b), world(x, b),
                                     (row + col) % 2 == 0 ? white : IM_COL32(15, 20, 28, 255));
            }
        }
    } else {
        draw->AddLine(world(0, -half), world(0, half), color, 2.0f);
    }
}

void car(ImDrawList* draw, const Camera& camera, const simulation::VehicleState& state, bool valid) {
    const double c = std::cos(state.heading_rad);
    const double s = std::sin(state.heading_rad);
    const auto point = [&](double forward, double left) {
        return camera.point({state.position_m.x + forward * c - left * s,
                             state.position_m.y + forward * s + left * c});
    };
    const ImU32 body = valid ? accent : IM_COL32(245, 151, 93, 255);
    draw->AddQuadFilled(point(-1.1, 1.05), point(3.2, 1.05), point(3.2, -1.05), point(-1.1, -1.05),
                         IM_COL32(13, 18, 25, 255));
    draw->AddQuadFilled(point(-0.9, 0.85), point(3.0, 0.85), point(3.0, -0.85), point(-0.9, -0.85), body);
    draw->AddQuadFilled(point(0.1, 0.65), point(1.7, 0.65), point(1.7, -0.65), point(0.1, -0.65),
                         IM_COL32(35, 61, 72, 255));
    draw->AddLine(point(2.5, 0.65), point(2.5, -0.65), white, 2.0f);
    draw->AddTriangleFilled(point(5.2, 0.0), point(3.8, 0.6), point(3.8, -0.6), white);
}

std::string time_text(std::chrono::microseconds time) {
    const auto ms = time.count() / 1000;
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld.%03lld",
                  static_cast<long long>(ms / 60000), static_cast<long long>((ms / 1000) % 60),
                  static_cast<long long>(ms % 1000));
    return buffer;
}

void scene(DrivingSession& session) {
    const auto origin = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const ImVec2 size{std::max(available.x, 1.0f), std::max(available.y, 1.0f)};
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(19, 34, 37, 255), 10);
    for (float x = origin.x; x < origin.x + size.x; x += 36.0f) {
        draw->AddLine({x, origin.y}, {x, origin.y + size.y}, IM_COL32(26, 43, 46, 255));
    }
    for (float y = origin.y; y < origin.y + size.y; y += 36.0f) {
        draw->AddLine({origin.x, y}, {origin.x + size.x, y}, IM_COL32(26, 43, 46, 255));
    }
    const auto& track = session.laps().track();
    const auto camera = fit_camera(track, origin, size);
    road_stroke(draw, track, camera, track.road_width_m() + 1.6, IM_COL32(99, 117, 118, 255));
    road_stroke(draw, track, camera, track.road_width_m(), asphalt);
    for (double progress = 0; progress < track.length_m(); progress += 5.0) {
        draw->AddLine(camera.point(track.pose_at(progress).position_m),
                      camera.point(track.pose_at(progress + 2.0).position_m),
                      IM_COL32(114, 129, 140, 255), 1.0f);
    }
    gate(draw, track, camera, 0.0, white, true);
    for (int i = 1; i <= 3; ++i) {
        const double progress = track.length_m() * static_cast<double>(i) / 4.0;
        gate(draw, track, camera, progress, IM_COL32(79, 161, 175, 255), false);
        const auto label = camera.point(track.pose_at(progress).position_m);
        const std::string text = "CP " + std::to_string(i);
        draw->AddText({label.x + 12, label.y - 22}, muted, text.c_str());
    }
    const auto position = camera.point(track.pose_at(0).position_m);
    draw->AddText({position.x - 150, position.y + 12}, white, "START / FINISH");
    const auto status = session.laps().status();
    car(draw, camera, session.render_state(), !status.running || status.current_lap_valid);
    const auto vehicle = camera.point(session.render_state().position_m);
    const ImVec2 marker{
        std::clamp(vehicle.x, origin.x + 24.0f, origin.x + size.x - 24.0f),
        std::clamp(vehicle.y, origin.y + 60.0f, origin.y + size.y - 45.0f)};
    const bool outside = vehicle.x != marker.x || vehicle.y != marker.y;
    draw->AddCircle(marker, 16.0f, IM_COL32(13, 18, 25, 255), 32, 5.0f);
    draw->AddCircle(marker, 16.0f, accent, 32, 2.0f);
    const char* label = outside ? "YOU - OFF MAP / R RESET" : "YOU";
    const auto text_size = ImGui::CalcTextSize(label);
    const float label_x = std::clamp(marker.x - text_size.x * 0.5f,
                                     origin.x + 8.0f, origin.x + size.x - text_size.x - 8.0f);
    draw->AddRectFilled({label_x - 5, marker.y - 46},
                         {label_x + text_size.x + 5, marker.y - 22}, IM_COL32(10, 20, 25, 240), 4);
    draw->AddText({label_x, marker.y - 43}, accent, label);
    if (outside) { draw->AddCircleFilled(marker, 5.0f, accent); }
    const ImVec2 title{origin.x + 22, origin.y + 18};
    draw->AddText(title, white, "PITWALL OVAL");
    draw->AddText({title.x, title.y + 23}, muted, "Top view  /  400 m circuit  /  drive counterclockwise");
    draw->AddText({title.x, origin.y + size.y - 30}, muted, "A steers left   D steers right   S brakes (no reverse)");
    if (session.paused()) {
        const ImVec2 p{origin.x + size.x * 0.5f - 120, origin.y + size.y * 0.5f - 35};
        draw->AddRectFilled(p, {p.x + 240, p.y + 70}, IM_COL32(10, 16, 25, 238), 10);
        draw->AddText({p.x + 88, p.y + 16}, accent, "PAUSED");
        draw->AddText({p.x + 35, p.y + 40}, white, "Press P or click Resume");
    }
    draw->PopClipRect();
    ImGui::Dummy(size);
}
} // namespace

void draw_simulator_ui(DrivingSession& session, simulation::ControlInput controls) {
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(display);
    ImGui::Begin("PitWall driving", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);
    ImGui::TextColored({0.56f, 0.91f, 0.62f, 1}, "PITWALL");
    ImGui::SameLine();
    ImGui::TextDisabled(" /  DRIVING SESSION");
    ImGui::SameLine(std::max(380.0f, display.x - 470.0f));
    ImGui::TextDisabled("WASD drive   P pause   R restart   ESC exit");
    ImGui::Separator();
    ImGui::BeginChild("Dashboard", {278, 0}, ImGuiChildFlags_Borders);
    if (ImGui::Button(session.paused() ? "Resume [P]" : "Pause [P]", {120, 32})) {
        session.set_paused(!session.paused());
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart [R]", {120, 32})) { session.restart(); }
    ImGui::Spacing();
    ImGui::TextDisabled("SPEED");
    ImGui::TextColored({0.56f, 0.91f, 0.62f, 1}, "YOU = GREEN CAR");
    ImGui::SetWindowFontScale(2.8f);
    ImGui::Text("%.0f", session.simulation().state().speed_mps * 3.6);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(); ImGui::TextDisabled("km/h");
    ImGui::Spacing();
    ImGui::TextDisabled("THROTTLE");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.56f, 0.91f, 0.62f, 1});
    ImGui::ProgressBar(static_cast<float>(controls.throttle), {-1, 9}, "");
    ImGui::PopStyleColor();
    ImGui::TextDisabled("BRAKE");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.95f, 0.45f, 0.40f, 1});
    ImGui::ProgressBar(static_cast<float>(controls.brake), {-1, 9}, "");
    ImGui::PopStyleColor();
    ImGui::Separator();
    const auto status = session.laps().status();
    ImGui::Text("LAP %zu", status.current_lap_number);
    ImGui::Text("%s", time_text(status.elapsed).c_str());
    ImGui::Text("Checkpoints: %zu / 3", status.passed_checkpoints);
    if (!status.running) {
        ImGui::TextWrapped("Cross the start line to begin timing.");
    } else if (status.current_lap_valid) {
        ImGui::TextColored({0.56f, 0.91f, 0.62f, 1}, "VALID LAP");
    } else {
        ImGui::TextColored({1, 0.61f, 0.40f, 1}, "INVALID LAP");
        ImGui::TextWrapped("%s", track::describe(status.reason).data());
    }
    const auto best = session.laps().best_lap_time();
    ImGui::Spacing();
    ImGui::TextDisabled("BEST VALID LAP");
    ImGui::Text("%s", best ? time_text(*best).c_str() : "--:--.---");
    ImGui::Separator();
    ImGui::TextDisabled("RECENT LAPS");
    const auto laps = session.laps().completed_laps();
    const auto first = laps.size() > 5 ? laps.size() - 5 : 0;
    for (std::size_t i = first; i < laps.size(); ++i) {
        ImGui::Text("%zu  %s  %s", laps[i].number, time_text(laps[i].duration).c_str(),
                     laps[i].valid ? "OK" : "INVALID");
    }
    if (laps.empty()) { ImGui::TextDisabled("No completed laps yet"); }
    ImGui::Spacing(); ImGui::Separator();
    ImGui::TextDisabled("LOCAL SIMULATION");
    ImGui::Text("100 Hz physics / %.0f FPS", ImGui::GetIO().Framerate);
    ImGui::Text("Session: %llu", static_cast<unsigned long long>(session.session_id()));
    ImGui::Text("Overrun: %.0f ms", std::chrono::duration<double, std::milli>(session.dropped_time()).count());
    ImGui::TextWrapped("Losing window focus pauses the session. Press P to resume.");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Circuit", {0, 0});
    scene(session);
    ImGui::EndChild();
    ImGui::End();
}

} // namespace pitwall::app
