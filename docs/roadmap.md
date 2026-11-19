# MVP roadmap

Implement one small, buildable slice at a time.

| Milestone | Acceptance criterion | Status |
| --- | --- | --- |
| Foundation | Two executables, domain library, CMake presets, offline CTest | Implemented |
| Dependency setup | Verify vcpkg/GoogleTest and commit registry baseline | Pending network access |
| Vehicle model | Fixed-step acceleration, braking, turning; independent tests | Implemented |
| Track and laps | One circuit; ordered checkpoints; directed finish crossing | Implemented |
| Interactive simulator | Drive a valid lap using WASD; pause and reset | Window/controls implemented; manual lap acceptance pending |
| UDP | Codec validation and loopback integration tests | Planned |
| Dashboard | Live values, speed/pedals charts, source diagnostics | Planned |
| Recording and replay | Save, reopen, pause, seek, change playback speed | Planned |
| MVP release | Windows build, documentation, demo recording, CI | Planned |

After MVP: autopilot, distance-aligned lap comparison, CSV, other tracks and external
simulator adapters. No opponent AI, track editor or detailed drivetrain before MVP.

## Next implementation slice

Manually verify the interactive session, then add a versioned UDP codec and sender
at 50 Hz plus an independent receiving pipeline. Define encodings, session metadata
and diagnostics before Analyzer UI integration. Preserve headless tests. GLFW
portability remains a separate adapter task; see docs/gui.md for the current choice.

## Risks

- Graphics and package setup can consume the project: keep offline domain builds
  available and add one dependency group at a time.
- Kinematic steering is not a grip model: state its limits and bound control inputs.
- UDP is lossy: expose gaps and incomplete recordings; never silently repair data.
- Rendering stalls must not produce a large integration step.
- Lap comparison needs common track geometry and progress, not raw travelled path.
- Shutdown and backpressure require tests before adding more threads.
