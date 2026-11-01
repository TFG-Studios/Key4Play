# Key4Play architecture

## Target baseline

- PS4 system software: 12.52 (owner-reported).
- GoldHEN: v2.4b18.9 is the provisional target inferred from the user's description; exact installed build remains unverified on-console.
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
2. Shared input snapshot: hand off keyboard/mouse state safely, neutralize on invalid reports/disconnects, and let the game-thread reader skip a sample rather than spin when the lock is busy.
3. Experimental controller overlay: load libScePad and detour scePadRead/scePadReadState with the GoldHEN x64 Detour engine; merge mapped buttons and mapped axes into the returned OrbisPadData while preserving untouched DS4 fields.
4. Title-specific compatibility testing; the overlay remains disabled by default and no global hook is enabled.

## Controller input boundary

OpenOrbis exposes scePadRead and scePadReadState and the OrbisPadData structure. GoldHEN's gamepad_helper demonstrates hooking those functions and editing returned pad state. This establishes a plausible game-facing route, but not universal compatibility. The current implementation explicitly loads and dynamically resolves libScePad exports, verifies the expected forwarding-stub signature, atomically swaps only the relative-jump displacement, and includes compile-time OrbisPadData layout checks. Unknown prologues are rejected rather than patched. This is cross-compiled but still needs runtime validation on the target GoldHEN build; it does not prove that every title uses these functions.

## Safety policy

- Never update/downgrade firmware for this project.
- Never install a PRX solely because it compiled.
- Start with USB reader diagnostics and title-specific plugin loading.
- The controller overlay is disabled by default; enabling it requires an explicit config flag and title-specific plugin entry.
- Do not manually unload the hooked plugin while a title is polling pad input; validate title-exit cleanup and keep a rollback path.
- Preserve the physical DS4.
