# Verification

Verified locally on Windows on 2026-11-13 (version 0.4.1):

| Configuration | Compiler | Result |
| --- | --- | --- |
| windows-mingw-gui, Debug | GNU C++ 16.2.0 / MSYS2 UCRT64 | Configured and built; 68/68 CTest tests passed; OpenGL smoke passed |
| windows-msvc-gui, Debug | MSVC 19.41.34123.0 / Visual Studio 2022 | Configured and built; 68/68 CTest tests passed; OpenGL smoke passed |
| windows-mingw, Debug (headless, previous 0.4.0) | GNU C++ 16.2.0 / MSYS2 UCRT64 | Previously configured and built; 68/68 CTest tests passed |

CMake version: 4.4.0-rc1. Builds produced no compiler warnings.

The domain contract checks valid defaults, session IDs, negative simulation time,
negative speed, pedal limits, valid negative coordinates/deceleration and NaN / both
infinities in every floating-point field. Checks remain active in Release builds.
Eight tests exercise startup, help, version and unknown arguments for both
applications. Twenty named physics tests verify rest, analytic acceleration and
stopping distance (including a stop inside a tick), forces, speed clamp, turn symmetry,
analytic circular motion, steering slew, curvature cap, invalid inputs/config/state,
frame scheduling at 30 vs 144 FPS, remainder preservation, overload, reset, telemetry,
atomic rejection and a 100-second turning run. One integration test runs the scripted
demo and requires completion after 1200 physics ticks.

The demo was also run manually: after 12 simulated seconds its final speed is 0 km/h,
travelled distance approximately 172.550 m and position approximately (148.989, 63.316) m.
It emits valid domain samples checked at each printed second.

Twenty-seven track tests cover perimeter, wrapping and projection, road boundaries,
swept corridor containment, rounded corners, invalid geometry/rules/samples, default
oval geometry, directed start, checkpoint order, lap time, best valid time, off-track
and shortcut invalidation, discontinuities, reverse crossings, exact line endpoints,
missing checkpoints, recovery, reset, interpolation, multiple crossings in one sample
and large timestamps. The final track test feeds the actual VehicleModel around a
30 m radius circular test track; it completes two valid laps with times matching the
analytic circle period within 0.1 ms.

An additional application test runs `--track-demo`: default oval length 400.274 m,
two valid laps at about 20.014 s each. This demo feeds scripted positions rather than
using a vehicle controller. Both existing physics demo and new track demo were run.

Not verified: Linux, the vcpkg/GoogleTest preset, or GitHub Actions execution.
GitHub access from the local execution environment failed with a DNS/thread error,
so external dependencies were not downloaded. No dependency baseline is claimed.
The GitHub Actions workflow is provided for future repository pushes.

Ten additional session tests exercise track spawn, per-tick output, resetting a
configured spawn, rejecting invalid spawn without mutation, pause, restart, equivalent
30/144 FPS scheduling, controls, invalid paused frames and render interpolation.

Both compiled GUI executables created an OpenGL 4.6 compatibility context, advanced
190 physics ticks and rendered 5 hidden frames to a 1280x800 framebuffer texture.
The framebuffer was read back to BMP; the image was inspected. A preview is included
at docs/images/simulator.png. This is a real application rendering, not a mockup.
The diagnostic uses an FBO to avoid undefined/clipped hidden-window swapchain readback.

No physical keyboard, native resize/focus scenario or interactive VS Code F5 session
was automated. Completing a valid lap by hand remains a user acceptance check.
Network, database, replay and an Analyzer GUI are not implemented.

0.4.1: MinGW and MSVC GUI builds passed all 68 CTest cases. Both executables also
ran --gui-window-smoke using the actual visible native-window backbuffer at
1280x800, with five frames and 190 scripted physics ticks. Top left/right edge
pixels are nonblank, and the complete rendered native frame and YOU marker were
visually inspected. This directly covers the native clipping reported by the user;
the previous FBO-only check did not. No changes to Windows display settings were made.
