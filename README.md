# Key4Play

**A standalone PS4 homebrew project for USB keyboard and mouse input.**

The final runtime must execute entirely on the PS4. A Windows PC is used only to develop and build the project; it is not part of the play-time setup.

## Non-negotiable requirements

- Direct USB keyboard and USB mouse connected to the PS4.
- No Remote Play.
- No PC, phone, external service, or external software running while playing.
- No USB keyboard/mouse adapter.
- GoldHEN may be used as the PS4 homebrew/plugin environment.
- Preserve the physical DualShock 4 wherever technically possible.
- All project changes are committed directly to main.

## Target baseline

- PS4 firmware: 12.52 (owner-reported).
- GoldHEN: v2.4b18.7 is the provisional target; installed build not yet verified on-console.
- Development host: Windows.
- Build automation: GitHub Actions for Windows Test Lab and PS4 PRX.

Do not update or downgrade the console for this project. Firmware 12.52 support does not guarantee every GoldHEN plugin API or hook is identical across builds.

## Current progress

### Windows Test Lab v0.2

- Compact resizable graphical application (1024x640 default).
- Visual DualShock-style controller preview, including sticks, D-pad, face buttons, bumpers, stick clicks, touchpad and trigger bars.
- WASD and arrow-key mapping.
- Face buttons, bumpers, stick clicks, Share and Options mappings.
- Mouse-to-right-stick and mouse-button-to-trigger mappings.
- Sensitivity, deadzone and acceleration controls.
- Shared C11 input core and automated tests.
- Generic HID descriptor parser with Report ID, keyboard array/NKRO and mouse field decoding (host-side fixtures).
- Lock-protected shared input snapshot with separate keyboard/mouse state, generation tracking, composition, and neutralization on invalid reports or disconnect.
- GitHub Actions Windows build and artifact packaging.

### PS4 USB HID reader

The PS4-side reader resolves sceUsbd, enumerates HID interfaces, claims interrupt-IN endpoints and starts independent keyboard/mouse reader threads. It now attempts to retrieve and parse HID report descriptors (including Report IDs and NKRO bitmaps), falling back to boot protocol for standard boot interfaces. Decoded state is published through the shared snapshot, with neutralization on invalid reports, read failures and disconnects.

**It still does not inject input into games.** No scePadRead/scePadReadState hooks or OrbisPadData modifications are enabled. The descriptor path is a first bounded implementation and still needs static review plus physical device testing.

The PRX compiles and is packaged by GitHub Actions. This validates compilation only, not runtime compatibility or safe console loading.

## Architecture

\`\`\`text
USB keyboard + USB mouse
          |
          v
     PS4 USB HID reader
          |
          v
    Shared Key4Play C11 input core
          |
          v
  Keyboard/mouse -> DualShock-style state
          |
          v
  Future game-facing controller integration
          |
          v
         Game
\`\`\`

The Windows Test Lab converts SDL input into synthetic USB HID boot reports, then runs those packets through the same boot-report decoders and mapping functions used by the PS4 reader. Synthetic descriptor fixtures also exercise Report-ID/NKRO parsing and the shared input snapshot. Windows SDL input is not itself the same as PS4 USB input; the report format and conversion logic are shared, while the real USB transport and hardware behavior are not.

## Repository layout

- shared/: portable C input core and automated tests.
- testlab/: Windows graphical simulator.
- ps4/plugin/: PS4 USB HID reader PRX (descriptor reports plus boot fallback).
- docs/: architecture, testing and compatibility notes.
- app/: future native PS4 configuration app.

## Development references

- [GoldHEN](https://github.com/GoldHEN/GoldHEN)
- [GoldHEN Plugin SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
- [GoldHEN Plugins Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository)
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
- [ProCon PS4 keyboard/mouse plugin](https://github.com/joojofc13-wq/ProCon)

## Roadmap

1. Test Lab v0.2 and automated mapping tests.
2. Complete static review and test generic HID report retrieval, descriptor reports and boot fallback.
3. Validate snapshot handoff, device hotplug and plugin shutdown on real PS4 hardware.
4. Verify OrbisPadData ABI and GoldHEN gamepad hooks against the installed GoldHEN build.
5. Add a title-specific controller overlay that preserves physical DS4 state.
6. Reversible console smoke tests and first gameplay validation.
7. Native PS4 configuration app, profiles and per-title compatibility.

## Compatibility expectations

The goal is broad compatibility, not a false promise of universal support. Mouse movement is converted to right-stick movement, not native mouse aiming. Camera acceleration, deadzones and turn-rate limits differ between games. Some games may use different controller initialization or read paths.

## Safety

- Never install an experimental PRX solely because it compiled.
- Begin with a reversible, title-specific reader-only test.
- Do not add game-facing hooks until USB input and controller merge have been validated.
- Keep the physical DS4 connected and maintain a rollback path.
