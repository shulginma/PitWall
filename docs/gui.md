# Interactive simulator (0.4.1)

## Scope and controls

Windows window, OpenGL rendering, ImGui scene/dashboard, WASD driving, pause,
restart, focus-loss/minimize pause, current/best lap, recent laps and visible overrun.
All drawing is procedural; no third-party image assets or fonts are needed.
There is no telemetry network connection yet. Analyzer remains a console entry point.

Run `--drive` from a GUI build. Spawn is two metres before the start, with heading
along the track tangent. W accelerates, S brakes, A/D steer, P pauses, R restarts,
Escape closes. There are no collisions or reverse gear. Use R to recover after
leaving the road. Lap validity still checks the rear axle point, not the car body.

## Build and platform decision

`PITWALL_BUILD_GUI` defaults OFF. `windows-mingw-gui` and `windows-msvc-gui` enable it.
The Windows CI job now compiles GUI code but runs headless tests only; Linux remains
headless. No package download is needed; a working OpenGL 3.0+ GPU driver is required.

The local GLFW example package had old 3.2 MSVC binaries and no source. Its MinGW
link probe failed. Those binaries are not shipped. Instead this stage uses Win32/WGL
plus the official ImGui Win32/OpenGL3 backends. This is a temporary portability
tradeoff. Replacing the adapter with GLFW later does not require replacing core logic.

Dear ImGui is vendored from the user's existing source snapshot: 1.91.5 WIP, with
unmodified original source, MIT license and SHA-256 hashes. The upstream commit is
not verified; this is not a latest-release claim. No GLFW, SDL, SFML or ImPlot is
linked at this stage. The renderer uses its upstream embedded OpenGL loader.

Primary integration references:
- https://github.com/ocornut/imgui/blob/master/examples/example_win32_opengl3/main.cpp
- https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md

## Responsibilities and lifetime

- DrivingSession owns physics, lap timing, pause, session ID and render alpha;
  its headers contain no Windows or ImGui types.
- gui_win32.cpp owns window, GL and ImGui lifetime via RAII, dispatches events,
  reads keys and runs frames. ImGui shuts down before the GL context is destroyed.
- gui_view.cpp renders dashboard, controls, fitted world-to-screen track and car.
- FixedStepSimulation exposes every executed state/time through last_ticks(),
  plus previous_state() for render-only interpolation. A zero-tick frame clears
  the tick stream without destroying the previous interpolation state.

DrivingSession passes EVERY physics tick to LapTracker, not just the final pose
of the render frame. Simulation stays at 100 Hz, with bounded catch-up and visible
dropped time. Rendering interpolates position/shortest heading without changing
authoritative physics. The main loop sleeps briefly rather than busy-spinning.

Pause produces no physics/lap updates and no duplicate lap timestamp. Restart
restores the configured spawn, clears clocks/history/best/remainder/overrun, unpauses
and increments the session ID. Focus loss/minimize pauses; regain does not auto-resume.
A stall longer than 60 seconds also pauses. There are no background threads yet.

## Verification

68 CTest cases: existing 58 plus 10 for spawn, per-tick output, validated reset,
pause, restart, frame partitioning, controls, invalid frames and interpolation.
MinGW and MSVC GUI builds and a MinGW headless build were exercised. Linux and
GoogleTest were not executed.

`--gui-smoke output.bmp` runs a scripted physical session, renders five hidden
frames to a 1280x800 FBO, saves the last frame and exits. The FBO avoids undefined
readback from occluded Windows swapchain pixels. It passed with both compilers using
an OpenGL 4.6 context. The actual rendered image was inspected.

The diagnostic does not automate physical keyboard input or a window manager.
Manual WASD driving, resize/focus behavior, completing a valid UI lap and the supplied
VS Code F5 integration remain acceptance checks for the user.

Next stage: versioned UDP telemetry, an independent receiver and then Analyzer UI.

## 0.4.1 native DPI fix

The original thread-level DPI helper did not ensure a consistent process/WGL pixel
space. The native window could display a magnified/clipped portion while an FBO
diagnostic still passed. The executable now embeds a PerMonitorV2 manifest as
resource 1 for both compilers, sets process awareness before window/GL creation,
handles WM_DPICHANGED and fits the initial window to the monitor work area.
Client rectangles are interpreted as physical pixels; framebuffer scale is 1.
CMake C++ warning flags are scoped to CXX so they do not leak to resource compilation.

`--gui-window-smoke output.bmp` briefly opens a visible topmost native window,
renders five scripted frames into its REAL default backbuffer, saves that frame
and exits. It does not use the diagnostic FBO. Both MinGW and MSVC passed at
1280x800; full native frame and car marker were inspected and edge pixels checked.
`--gui-smoke` remains the hidden FBO diagnostic. Neither automates physical keys.

Car highlighting: a 16-pixel ring plus YOU label. When the car is outside the map,
the marker is clamped to the map edge and offers R RESET. No physics coordinates
are changed for visibility. The camera still fits the whole track.

Reference: https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
