# Key4Play Test Lab

A Windows-only development utility for safely exercising Key4Play's portable input-processing core. It is not part of the final PS4 runtime and is never required while playing.

## Current features

- Live WASD-to-left-stick preview.
- Mouse-delta-to-right-stick preview.
- Adjustable sensitivity, deadzone, and acceleration.
- Optional relative mouse capture (press Escape to release).
- Diagnostics tab.
- Portable input core with unit tests.

## Important limitations

This is a simulator, not a virtual gamepad. It does not inject input into games, install drivers, connect to a PS4, load GoldHEN plugins, or emulate native mouse support. Smoothing, profile persistence, custom bindings, report export, and real controller integration are future work.

## Build locally on Windows

Requirements: Git, CMake 3.20+, and Visual Studio 2022 Build Tools (Desktop development with C++).

From the repository root:

\`\`\`powershell
cmake -S testlab -B testlab/build -A x64 -DBUILD_TESTING=ON
cmake --build testlab/build --config Release --parallel
ctest --test-dir testlab/build -C Release --output-on-failure
\`\`\`

The executable is produced under \`testlab/build/Release/\`.

CMake downloads pinned SDL2 and Dear ImGui source revisions during configuration. An internet connection is required for the first configure.

## GitHub Actions

The workflow at \`.github/workflows/testlab-windows.yml\` builds the x64 Windows app and runs the portable-core tests on pushes to \`main\` that touch Test Lab files, on matching pull requests, and when manually dispatched. A ZIP artifact is uploaded for 14 days.

## Relationship to Key4Play

The input core is intentionally independent of SDL and Dear ImGui. The long-term goal is to share or port this deterministic processing layer into the PS4-side plugin after the real GoldHEN/PS4 integration APIs have been verified. The GUI is strictly a Windows development tool.