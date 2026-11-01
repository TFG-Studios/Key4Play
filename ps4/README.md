# Key4Play PS4-side development

## Current milestone: USB boot-HID reader (no game injection)

The PS4 PRX now:
1. Loads the USB system module and resolves sceUsbd functions dynamically.
2. Enumerates connected USB devices and active configuration descriptors.
3. Identifies USB HID boot keyboard and boot mouse interfaces.
4. Finds an interrupt-IN endpoint and records its packet size.
5. Opens and claims the selected interface.
6. Requests HID boot protocol and starts separate keyboard and mouse reader threads.
7. Reads interrupt reports, decodes boot keyboard/mouse packets through the shared C11 core, and logs the mapped DualShock-style state.
8. Releases claimed interfaces and closes handles on disconnect/unload.

This is the first real PS4 input-reading path. It is still deliberately diagnostic-only: it does not hook scePadRead/scePadReadState, modify OrbisPadData, create a virtual controller, or send mapped input to games.

Only standard HID boot keyboard and boot mouse interfaces are supported at this stage. NKRO, report-ID, vendor-defined and descriptor-defined gaming peripherals are not yet supported.

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
1. Validate boot keyboard/mouse report reading and disconnect/reconnect behavior on PS4.
2. Add a host-tested HID report-descriptor parser for non-boot devices.
3. Add a bounded, thread-safe input snapshot and report diagnostics.
4. Verify the exact OrbisPadData ABI and GoldHEN hook behavior for the target firmware/build.
5. Add controller merge only after USB reading is stable.
