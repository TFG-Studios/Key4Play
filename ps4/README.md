# Key4Play PS4-side development

## Current milestone: USB descriptor discovery only

The initial PS4-side scaffold uses the OpenOrbis sceUsbd interface to:
1. Load/check the USB system module and resolve the USB functions.
2. Initialize sceUsbd.
3. Enumerate connected USB devices.
4. Read device descriptors and active configuration descriptors.
5. Identify HID boot keyboard and boot mouse interface descriptors.
6. Log the scan summary and release descriptor/list allocations.

It deliberately does not open devices, claim interfaces, submit transfers, read input reports, hook scePadRead, patch game memory, or synthesize controller input. This is a safe API/build reconnaissance stage, not a usable keyboard/mouse plugin.

The source resolves the USB and sysmodule entry points dynamically, following the approach used by existing PS4 USB homebrew. OpenOrbis declares the libusb-compatible structures under orbis/_types/libusb.h; the local include/libusb.h forwards the include expected by Usbd.h.

## Build prerequisites

- OpenOrbis PS4 Toolchain (LLVM 18 release recommended by the current toolchain release).
- GoldHEN Plugins SDK.
- Clang, LLD and GNU Make.
- Environment variables:
  - OO_PS4_TOOLCHAIN: extracted OpenOrbis toolchain root.
  - GOLDHEN_SDK: GoldHEN Plugins SDK checkout.

Build from the repository root:

    make -C ps4/plugin

Expected output: ps4/plugin/build/key4play_usb_probe.prx.

The build recipe and PRX packaging have not yet been validated by GitHub Actions. Do not install this output on the console until CI validates the build and the plugin's exact loading/rollback procedure is reviewed.

## Safety and next steps

The plugin only enumerates descriptors when explicitly loaded. It does not poll USB interfaces. Before adding polling, validate device-list lifetime, endpoint metadata, interface claim/release, interrupt transfers, report descriptors, and unload synchronization on firmware 12.52 / the installed GoldHEN build.

Next safe increments:
1. Compile this source against pinned SDK/toolchain revisions in GitHub Actions.
2. Add a host-testable HID report descriptor parser.
3. Implement PS4 interface open/claim and short bounded report reads with strict cleanup.
4. Only after USB reading is stable, research and implement the game-facing controller merge.
