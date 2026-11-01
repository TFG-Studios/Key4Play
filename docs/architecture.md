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

The shared parser supports bounded descriptor-defined layouts, Report IDs, keyboard arrays/NKRO bitmaps, mouse buttons, signed X/Y axes and vertical wheel. The PS4 USB reader now requests report descriptors and attempts this path before falling back to boot protocol. It currently opens one keyboard-class and one mouse-class interface independently; a single physical interface exposing both collections, multiple same-class devices, vendor-defined reports and unusual composite layouts remain limitations.

## PS4 integration stages

1. USB HID reader: resolve sceUsbd, enumerate interfaces, retrieve bounded report descriptors, decode descriptor-defined or boot reports on independent reader threads, and publish snapshots.
2. Shared input snapshot: hand off keyboard/mouse state safely and neutralize on invalid reports or disconnects.
3. Controller overlay: use the GoldHEN hook model for scePadRead/scePadReadState only after confirming exact target ABI, symbol resolution, hook lifecycle and physical DS4 preservation.
4. Title-specific compatibility testing; no global hook by default.

## Controller input boundary

OpenOrbis exposes scePadRead and scePadReadState and the OrbisPadData structure. GoldHEN's gamepad_helper demonstrates hooking those functions and editing returned pad state. This establishes a plausible game-facing route, but not universal compatibility. The actual implementation must resolve the target module in the game process, validate hook creation, preserve all original fields, and restore hooks cleanly. Runtime validation on the target GoldHEN build is mandatory.

## Safety policy

- Never update/downgrade firmware for this project.
- Never install a PRX solely because it compiled.
- Start with USB reader diagnostics and title-specific plugin loading.
- No controller hooks enabled in a user-facing build until USB input and shared-core tests pass, hook lifecycle is reviewed, and a rollback path is ready.
- Preserve the physical DS4.
