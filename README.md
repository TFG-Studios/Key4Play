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
- Generic HID descriptor parser with Report ID, keyboard array/NKRO and mouse field decoding (host-side fixtures; not yet connected to PS4 USB reader).
- GitHub Actions Windows build and artifact packaging.

### PS4 USB boot-HID reader

The first PS4-side reader now resolves sceUsbd dynamically, enumerates USB HID interfaces, finds interrupt-IN endpoints, claims boot keyboard/mouse interfaces, starts separate reader threads, reads HID reports and converts them through the shared input core into diagnostic DualShock-style state.

**It still does not inject input into games.** No scePadRead/scePadReadState hooks or OrbisPadData modifications are enabled. It supports standard boot-protocol keyboard/mouse interfaces only; NKRO and vendor-specific gaming peripherals need a generic HID descriptor parser.

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

The Windows Test Lab converts SDL input into synthetic USB HID boot reports, then runs those packets through the same boot-report decoders and mapping functions used by the PS4 reader. Windows SDL input is not itself the same as PS4 USB input; the report format and conversion logic are shared, while the real USB transport and hardware behavior are not.

## Repository layout

- shared/: portable C input core and automated tests.
- testlab/: Windows graphical simulator.
- ps4/plugin/: PS4 USB boot-HID reader PRX.
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
2. PS4 USB boot-HID report reading and hotplug validation.
3. Integrate the generic HID parser into the PS4 USB reader and validate descriptor/report requests.
4. Thread-safe shared input snapshots and device profiles.
5. Verify OrbisPadData ABI and GoldHEN gamepad hooks.
6. Merge keyboard/mouse state with physical DS4 input, initially title-specific.
7. Native PS4 configuration app, profiles and per-title compatibility.

## Compatibility expectations

The goal is broad compatibility, not a false promise of universal support. Mouse movement is converted to right-stick movement, not native mouse aiming. Camera acceleration, deadzones and turn-rate limits differ between games. Some games may use different controller initialization or read paths.

## Safety

- Never install an experimental PRX solely because it compiled.
- Begin with a reversible, title-specific reader-only test.
- Do not add game-facing hooks until USB input and controller merge have been validated.
- Keep the physical DS4 connected and maintain a rollback path.
