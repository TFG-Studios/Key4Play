# Key4Play

**A standalone PS4 homebrew project for USB keyboard and mouse input.**

Key4Play's runtime must execute entirely on the PS4. A Windows PC is used only to develop/build the project; it is not part of the play-time setup.

## Non-negotiable project requirements

- Direct USB keyboard and USB mouse connected to the PS4.
- No Remote Play.
- No PC, phone, external service, or external software running while playing.
- No USB keyboard/mouse adapter.
- GoldHEN may be used as the PS4 homebrew/plugin environment.
- Physical DualShock 4 support should be preserved where technically possible.
- All project changes are committed directly to `main`.

## Current milestone: Phase 1 — plugin loader probe

The first deliverable is a minimal GoldHEN PRX based on the upstream GoldHEN plugin template. It only reports that it loaded and displays a notification. It does not read USB input, intercept controller calls, patch game memory, or emulate a controller.

This milestone verifies the toolchain and plugin-loading path before any input work begins.

## Firmware status

- Console firmware reported by the owner: **12.52**
- GoldHEN version: **not yet identified**
- DualShock 4 available: **yes**
- Development OS: **Windows**

Do not update or downgrade the console for this project. Identify the complete GoldHEN version/build string in the GoldHEN menu before attempting to load a custom plugin. Plugin compatibility must be verified against the exact installed build.

## Architecture

```text
USB keyboard + USB mouse
          |
          v
   PS4 USB HID layer
          |
          v
   Key4Play input engine (GoldHEN PRX)
          |
          v
   Mapping + mouse processing
          |
          v
   Game-facing controller input integration
          |
          v
         Game

Separate PS4 homebrew configuration app:
profiles, bindings, sensitivity, device status, diagnostics
```

The game-facing controller integration mechanism is deliberately not selected yet. We must inspect available PS4 APIs and existing GoldHEN examples before choosing a method. The final implementation must not depend on an external PC or adapter.

## Repository layout

- `plugin/key4play_probe/`: Phase 1 GoldHEN PRX probe.
- `app/`: future native PS4 configuration application.
- `config/`: profile/configuration schema.
- `docs/`: architecture, setup, firmware notes, and test plan.
- `scripts/`: Windows build helper.

## Development references

- [GoldHEN Plugin SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
- [GoldHEN Plugins Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository)
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)

The GoldHEN Plugin SDK requires the OpenOrbis PS4 Toolchain and LLVM 10+. Its README recommends building the SDK source alongside plugins when practical, or using `build_static.bat` on Windows for a static library.

## Phase 1

1. Install Git, OpenOrbis PS4 Toolchain, and LLVM on Windows.
2. Set `OO_PS4_TOOLCHAIN` to the toolchain installation directory.
3. Run `scripts/build.ps1`.
4. Confirm `dist/key4play_probe.prx` is produced.
5. Identify the installed GoldHEN version on the PS4.
6. Review the exact plugin-loading instructions for that GoldHEN build.
7. Test only with a reversible, title-specific plugin configuration entry.

**Do not install the PRX until the GoldHEN version is confirmed and the build output has been reviewed.**

## Roadmap

1. Phase 1: build/load verification.
2. Phase 2: USB HID device discovery and report parsing.
3. Phase 3: key mapping and mouse-to-stick processing.
4. Phase 4: game-facing controller integration and compatibility testing.
5. Phase 5: native PS4 configuration application and profiles.

## Compatibility expectations

The goal is broad game compatibility, not a false promise of universal support. Translating mouse movement into a controller's right analog stick is not equivalent to native mouse aiming; game camera acceleration, deadzones, and turn-rate limits can affect the feel. Some titles may also use different controller initialization/input paths.

## Safety

A faulty plugin can crash a game or console session. Start with a harmless probe, use title-specific loading first, keep backups of GoldHEN configuration files, and avoid global hooks until they are necessary and tested.
