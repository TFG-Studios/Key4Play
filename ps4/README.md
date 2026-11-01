# Key4Play PS4-side development

## Current implementation

The experimental PRX currently contains:

1. Dynamic loading/resolution of the USB system module and sceUsbd functions.
2. USB HID interface enumeration and interrupt-IN endpoint discovery.
3. Bounded HID report-descriptor retrieval and parsing where the interface exposes a report descriptor.
4. Descriptor-defined Report-ID/NKRO keyboard and mouse decoding, with standard boot-protocol fallback.
5. Independent keyboard and mouse reader threads.
6. A lock-protected shared input snapshot; invalid reports, mouse idle timeouts, read failures and disconnects publish neutral state.
7. An opt-in gamepad overlay that detours `scePadRead` and `scePadReadState`, merges mapped button bits, overrides only mapped axes, and preserves other physical DS4 fields.

The overlay uses the pinned GoldHEN SDK Detour implementation and OpenOrbis `OrbisPadData`. Compile-time checks cover the fields used by the overlay and the expected structure size. The PRX links `libScePad`, the SDK Detour/HDE64 sources, and the shared HID/input modules.

## Controller overlay safety gate

The controller overlay is **disabled by default**. It is only enabled if `/data/GoldHEN/key4play.ini` contains:

```ini
[Key4Play]
enable_hooks=1
```

The PRX must additionally be loaded from a **specific title section** in GoldHEN's `plugins.ini`, never from `[default]`. The example configuration is `ps4/key4play.ini.example` and keeps the setting disabled.

Do not enable the overlay or install this build yet. The hooks have only been cross-compiled. They have not been validated in a running title on firmware 12.52, and we have not verified the imported pad-function targets, detour stability, DS4 passthrough, device compatibility or unload behavior on hardware. In particular, do not manually unload a hooked plugin while a game is actively polling controller input.

## Input mapping

The shared mapping currently uses:
- WASD: left stick.
- Arrow keys: D-pad.
- Space: Cross; Left Ctrl: Circle; E: Square; Q: Triangle.
- Left Shift/R: L1/R1; C/V: L3/R3; Enter: Options.
- Mouse movement: right stick; left/right mouse buttons: R2/L2; middle mouse button: touchpad.
- Mouse stick state is neutralized on USB idle timeout to avoid a stuck deflection.

Mouse-to-right-stick is controller emulation, not native mouse aiming. Sensitivity and response still need real-game tuning. The current USB transport selects one qualifying keyboard interface and one qualifying mouse interface; multiple devices of the same class, a single interface exposing both collections, unusual composite devices, vendor-specific report formats and physical-device quirks remain limitations.

## Build prerequisites

- OpenOrbis PS4 Toolchain v0.5.4 / LLVM 18.
- GoldHEN Plugins SDK (the CI workflow pins a tested revision).
- Clang, LLD and GNU Make.
- `OO_PS4_TOOLCHAIN`: extracted OpenOrbis toolchain root.
- `GOLDHEN_SDK`: GoldHEN Plugins SDK checkout.

Build from the repository root:

    make -C ps4/plugin

Expected output: `ps4/plugin/build/key4play_usb_probe.prx`.

GitHub Actions cross-compiles the PRX and uploads an artifact. A successful build proves compilation and linking only; it does not prove runtime compatibility or safe installation.

## Remaining validation before a user test build

1. Validate the USB descriptor requests and reader lifecycle on the actual console.
2. Confirm the imported `scePadRead` / `scePadReadState` targets are the intended game-process functions on the installed GoldHEN build.
3. Validate detour installation/removal and DS4 passthrough in a controlled title.
4. Verify real keyboard/mouse models, unplug/replug behavior and mouse response.
5. Prepare a reversible, title-specific test procedure with a known rollback path.

Until those steps are complete, treat every PRX artifact as an experimental engineering build, not an installable gameplay release.
