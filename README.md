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

- Compact resizable graphical application.
- Visual DualShock-style controller preview.
- WASD and arrow-key mapping.
- Face buttons, bumpers, stick clicks, Share and Options mappings.
- Mouse-to-right-stick and mouse-button-to-trigger mappings.
- Sensitivity, deadzone and acceleration controls.
- Shared C11 input core and automated tests.
- GitHub Actions Windows build and artifact packaging.

### PS4 USB discovery scaffold

The initial PS4 PRX scaffold dynamically resolves the USB system module functions and enumerates USB device/configuration descriptors. It identifies HID boot keyboard and mouse interfaces.

**It does not yet open devices, claim interfaces, read reports, hook controller functions, or inject inputs.** It is a discovery-only build milestone, not an installable Key4Play release.

The PS4 build workflow downloads the pinned OpenOrbis toolchain and GoldHEN SDK headers/CRT, then compiles and packages the discovery-only PRX. The initial PRX build has passed GitHub Actions. This validates compilation only, not runtime compatibility or safe console loading.

## Architecture

\`\`\`text
USB keyboard + USB mouse
          |
          v
     PS4 USB HID
          |
          v
    Shared Key4Play C11 input core
          |
          v
  Keyboard/mouse -> DualShock-style state
          |
          v
  Game-facing controller integration
          |
          v
         Game
\`\`\`

The Windows Test Lab simulates USB HID boot reports and uses the same portable mapping code intended for the PS4 plugin. Windows SDL input is not itself the same as PS4 USB input; it only creates test reports for the shared core.

## Repository layout

- shared/: portable C input core and automated tests.
- testlab/: Windows graphical simulator.
- ps4/plugin/: PS4 USB descriptor discovery PRX scaffold.
- docs/: architecture, testing and compatibility notes.
- app/: future native PS4 configuration app.

## Development references

- [GoldHEN](https://github.com/GoldHEN/GoldHEN)
- [GoldHEN Plugin SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
- [GoldHEN Plugins Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository)
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
- [ProCon PS4 keyboard/mouse plugin](https://github.com/joojofc13-wq/ProCon)

## Roadmap

1. Test Lab v0.2: improve input preview and automated mapping tests.
2. USB discovery: compile and validate the PS4 descriptor discovery scaffold.
3. HID descriptor parser: support boot and descriptor-defined keyboard/mouse reports.
4. PS4 USB reader: open/claim HID interfaces, read reports, handle hotplug and cleanup.
5. Controller conversion: share mapping and merge logic with the PS4 plugin.
6. Game-facing integration: verify Pad ABI and GoldHEN hooks, then merge input while preserving DS4.
7. Native PS4 configuration app, profiles and per-title compatibility.

## Compatibility expectations

The goal is broad compatibility, not a false promise of universal support. Mouse movement is converted to right-stick movement, not native mouse aiming. Camera acceleration, deadzones and turn-rate limits differ between games. Some games may use different controller initialization or read paths.

## Safety

- Never install an experimental PRX solely because it compiled.
- Start with discovery-only, title-specific plugin loading and a rollback plan.
- Do not add global hooks until USB input and controller merge have been validated.
- Keep the physical DS4 connected.
