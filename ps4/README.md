# Key4Play PS4-side development

## Current milestone: USB boot-HID reader (no game injection)

The PS4 PRX now:
1. Loads the USB system module and resolves sceUsbd functions dynamically.
2. Enumerates connected USB devices and active configuration descriptors.
3. Identifies USB HID boot keyboard and boot mouse interfaces.
4. Finds an interrupt-IN endpoint and records its packet size.
5. Opens and claims the selected interface.
6. Reads HID class descriptors and requests bounded report descriptors where available.
7. Parses descriptor-defined report layouts and reads Report-ID/NKRO keyboard and mouse reports; falls back to boot protocol for standard boot interfaces.
8. Starts separate keyboard and mouse reader threads, publishes decoded states into the shared snapshot, and neutralizes a device state on invalid reports, read failures and disconnects.
9. Releases claimed interfaces and closes handles on disconnect/unload.

This is the first real PS4 input-reading path. It is still deliberately diagnostic-only: it does not hook scePadRead/scePadReadState, modify OrbisPadData, create a virtual controller, or send mapped input to games.

The PS4 transport now attempts descriptor-defined HID reports first, including Report IDs and NKRO key bitmaps, and falls back to standard boot protocol when descriptor retrieval/parsing is unavailable. This remains a bounded first implementation: vendor-specific layouts, unusual composite interfaces, multiple devices of the same class, and physical-device quirks still require testing.

## Build prerequisites

- OpenOrbis PS4 Toolchain v0.5.4 / LLVM 18.
- GoldHEN Plugins SDK (workflow pins a tested revision).
- Clang, LLD and GNU Make.
- Environment variables:
  - OO_PS4_TOOLCHAIN: extracted OpenOrbis toolchain root.
  - GOLDHEN_SDK: GoldHEN Plugins SDK checkout.

Build from the repository root:

    make -C ps4/plugin

Expected output: ps4/plugin/build/key4play_usb_probe.prx.

GitHub Actions builds this PRX from the pinned toolchain/SDK and uploads an artifact. Compilation is not proof of runtime compatibility, correct USB permissions, stable thread behavior or safe console loading. Do not install it on the console until the code has been reviewed and a reversible, title-specific test plan is ready.

## Safety and next steps

The plugin performs USB reads only when explicitly loaded. It does not inject controller input. Before hardware testing, verify plugin lifecycle and rollback. The first console test should be limited to loading/unloading and device-reader diagnostics, with the physical DS4 connected.

Next increments:
1. Run static and synthetic tests for descriptor-defined reports, boot fallback and snapshot disconnect neutralization.
2. Review PS4 USB descriptor retrieval and lifecycle assumptions against the target GoldHEN/firmware build.
3. Validate the controller overlay's imported `scePadRead` targets and detour behavior in a controlled title on the target firmware.
4. Confirm physical DS4 passthrough and input coexistence; tune mouse-to-stick behavior.
5. Perform reversible PS4 smoke tests before describing any PRX as a gameplay release.
