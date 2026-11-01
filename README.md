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
- Face buttons, bumpers, stick clicks and Options mappings. Tab/Share is intentionally unmapped because the normal PS4 Pad API does not expose Share as an ordinary game button.
- Mouse-to-right-stick and mouse-button-to-trigger mappings.
- Sensitivity, deadzone and acceleration controls.
- Shared C11 input core, HID parser, input snapshot and signature-checked Pad jump planner with automated tests.
- Generic HID descriptor parser with Report ID, keyboard array/NKRO and mouse field decoding (host-side fixtures).
- Lock-protected shared input snapshot with separate keyboard/mouse state, generation tracking, composition, and neutralization on invalid reports or disconnect.
- GitHub Actions Windows build and artifact packaging.

### PS4 USB HID reader and experimental controller overlay

The PS4-side reader resolves sceUsbd, enumerates HID interfaces, retrieves bounded HID report descriptors where available, and decodes Report-ID/NKRO reports with boot-protocol fallback. Decoded state is published through a shared snapshot and neutralized after invalid reports, read failures, mouse idle timeouts and disconnects.

An opt-in `scePadRead` / `scePadReadState` overlay is now included. It explicitly loads `libScePad`, uses signature-checked 64-bit relative-jump hooks, adds mapped buttons and only overrides mapped stick axes, leaving other physical DS4 fields intact. Hook installation checks the known Pad forwarding-stub signature and atomically changes only its relative-jump displacement; unknown prologues are rejected instead of patched. The shared snapshot read is non-blocking on the game polling path. The overlay is **disabled by default** and requires `/data/GoldHEN/key4play.ini` with `[Key4Play]` and `enable_hooks=1`. It must also be loaded only in a specific title section of GoldHEN `plugins.ini`, never `[default]`.

**Important:** the overlay and USB reader have only been cross-compiled, not run on PS4 hardware. Hook target resolution, forwarding-stub signature compatibility, physical DS4 passthrough, USB device compatibility and safe unload remain unverified. Do not install or enable this build yet.

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
  Experimental signature-checked scePadRead overlay
          |
          v
         Game
\`\`\`

The Windows Test Lab converts SDL input into synthetic USB HID boot reports, then runs those packets through the same boot-report decoders and mapping functions used by the PS4 reader. Synthetic descriptor fixtures also exercise Report-ID/NKRO parsing and the shared input snapshot. Windows SDL input is not itself the same as PS4 USB input; the report format and conversion logic are shared, while the real USB transport and hardware behavior are not.

## Repository layout

- shared/: portable C input core and automated tests.
- testlab/: Windows graphical simulator.
- ps4/plugin/: PS4 USB HID reader plus opt-in experimental controller overlay PRX.
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
4. Validate the experimental `scePadRead` / `scePadReadState` overlay, forwarding-stub targets and DS4 passthrough on the target GoldHEN build.
5. Tune mouse-to-stick response and validate real keyboards/mice.
6. Reversible console smoke tests and first gameplay validation.
7. Native PS4 configuration app, profiles and per-title compatibility.

## Compatibility expectations

The goal is broad compatibility, not a false promise of universal support. Mouse movement is converted to right-stick movement, not native mouse aiming. Camera acceleration, deadzones and turn-rate limits differ between games. Some games may use different controller initialization or read paths.

## Safety

- Never install an experimental PRX solely because it compiled.
- Begin with a reversible, title-specific reader-only test.
- Keep the experimental game-facing hooks disabled until their runtime behavior has been validated.
- Keep the physical DS4 connected and maintain a rollback path.
