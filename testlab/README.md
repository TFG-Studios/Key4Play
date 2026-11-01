# Key4Play Test Lab

Windows-only development utility for exercising the portable Key4Play input core. It is not part of the final PS4 runtime and is never needed while playing.

## Version 0.2

- Compact resizable window (1024x640 default; 800x520 minimum).
- DualShock-style visual preview for sticks, D-pad and face buttons.
- Keyboard mappings for sticks, D-pad, face buttons, bumpers, stick clicks, Share and Options.
- Mouse movement to right-stick conversion; left/right/middle clicks to R2/L2/touchpad.
- Sensitivity, deadzone and acceleration controls.
- Live controller button bitmask, stick axes and trigger values.
- Shared C11 input core under shared/, intended for use by Windows and the PS4 plugin.

## Build locally

Requirements: Git, CMake 3.20+, Visual Studio C++ workload.

\`\`\`powershell
cmake -S testlab -B testlab/build -A x64 -DBUILD_TESTING=ON
cmake --build testlab/build --config Release --parallel
ctest --test-dir testlab/build -C Release --output-on-failure
\`\`\`

CMake downloads pinned SDL2 and Dear ImGui revisions on first configure.

## Important limitations

The current Test Lab uses SDL to simulate HID reports. The portable core decodes USB HID boot keyboard/mouse reports and maps them into a DualShock-style state. It does not create a Windows virtual gamepad, inject into games, load a GoldHEN plugin, or connect to a PS4.

The boot report parser is intentionally limited to standard boot-protocol keyboard/mouse report layouts. Gaming peripherals using vendor-specific or report-descriptor-defined layouts will need descriptor parsing and per-device validation.

The PS4-side milestone will first enumerate USB devices and identify candidate HID interfaces. It will not install game hooks or modify game memory.

## GitHub Actions

The Windows workflow builds the x64 GUI, runs shared-core unit tests, and uploads a ZIP artifact for 14 days on matching changes to main.

## Shared core

shared/include/key4play/input_core.h and shared/src/input_core.c contain the C-compatible input model and mapping logic. Keep this module free of SDL, ImGui and PS4-specific types so it can be compiled by both desktop and PS4 toolchains.
