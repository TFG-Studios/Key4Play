# Key4Play architecture

## Target baseline

- PS4 system software: 12.52 (owner-reported).
- GoldHEN: v2.4b18.7 is the provisional target, not yet verified on-console.
- Build host: Windows; GitHub Actions builds the Windows Test Lab and PS4 PRX.
- Runtime requirement: all final input handling runs on PS4. The Windows Test Lab is development-only.

## Shared input core

The portable C11 module in shared/ owns:
- USB HID boot keyboard/mouse report decoding.
- Keyboard usage -> DualShock-style button/stick mapping.
- Mouse delta -> right-stick conversion.
- Mouse buttons -> trigger/touchpad-click mapping.
- Merge rules that preserve untouched physical DS4 state.

It has no dependency on SDL, ImGui, GoldHEN or Orbis types. Test Lab simulates reports and calls the same functions used by the PS4 reader.

The current decoder supports standard boot keyboard and mouse reports only. Non-boot gaming keyboards, NKRO reports, report IDs, vendor-defined mouse reports and composite-device quirks need descriptor parsing and device-specific validation.

## PS4 integration stages

1. USB boot-HID reader (ps4/plugin): resolve sceUsbd, enumerate devices, locate interrupt-IN endpoints, claim boot keyboard/mouse interfaces, read reports on separate threads, decode and log mapped states. No game hooks.
2. Generic HID: implement report-descriptor parsing and add support for report IDs/NKRO/vendor-defined layouts.
3. Controller merge: verify exact OrbisPadData ABI and GoldHEN hook APIs for the target build. Preserve physical DS4 fields and add only mapped inputs.
4. Title-specific compatibility testing; no global hook by default.

## Controller input boundary

OpenOrbis exposes scePadRead and scePadReadState and the OrbisPadData structure. GoldHEN's gamepad_helper demonstrates hooking those functions and editing returned pad state. This is evidence for a possible game-facing integration point, not proof that a single hook works for every game or GoldHEN build. We must confirm function signatures, hook return semantics, library loading and target module addresses against the actual SDK and firmware.

## Safety policy

- Never update/downgrade firmware for this project.
- Never install a PRX solely because it compiled.
- Start with USB reader diagnostics and title-specific plugin loading.
- No controller hooks until USB input and shared-core tests pass and a rollback path is ready.
- Preserve the physical DS4.
