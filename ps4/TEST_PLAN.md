# Key4Play PS4 validation plan

This is a staged validation plan for the experimental PRX. It is not an installation guide for a gameplay release. The working target is GoldHEN v2.4b18.9 on firmware 12.52, inferred from the owner's description of the 13.00-capable release; confirm the actual GoldHEN version in the console menu before proceeding.

## Before touching the console

Confirm:
- PS4 firmware is 12.52.
- Exact GoldHEN version/build shown in the GoldHEN menu.
- The title ID of one low-risk test title that polls the normal Pad API.
- A working DualShock 4 is available for recovery/control.

Do not proceed if the installed GoldHEN build is not confirmed to support firmware 12.52.

## Stage 1 — reader-only smoke test

The PRX starts USB readers even when controller hooks are disabled. Keep the configuration explicitly disabled:

```ini
[Key4Play]
enable_hooks=0
```

1. Back up `/data/GoldHEN/plugins.ini` before editing it.
2. Put the PRX in `/data/GoldHEN/plugins/key4play.prx`.
3. Add its path only under the chosen test title's section in `plugins.ini`; do not use `[default]`.
4. Keep the DS4 connected.
5. Launch the test title and inspect GoldHEN/plugin log output for Key4Play startup and USB interface messages.
6. Confirm the title and DS4 continue behaving normally.
7. Connect one standard USB keyboard and one standard USB mouse; check whether the reader identifies both without crashing or disconnecting the DS4.

At this stage, keyboard and mouse input is diagnostic only; it must not change game controls.

## Stage 2 — signature-gated controller overlay

Proceed only after Stage 1 has completed without errors and the exact GoldHEN build is known.

Change the config to:

```ini
[Key4Play]
enable_hooks=1
```

The plugin resolves `scePadRead` and `scePadReadState` from `libScePad`. It installs hooks only if both exports match the expected aligned forwarding-stub signature and the relative jump fits. A mismatch is a deliberate fail-closed result: the plugin should report the rejected signature and not patch an unknown function layout.

Do not attempt to bypass a signature rejection or force the hook. It means the current firmware/build's Pad entry points need to be inspected before proceeding.

If hooks install, test one control at a time:
- WASD movement.
- Face buttons and D-pad.
- Mouse movement and mouse buttons.
- DS4 input simultaneously with keyboard/mouse.
- Disconnect and reconnect keyboard/mouse.

Stop immediately if the game freezes, the DS4 stops responding, or the plugin reports a failed hook restore.

## Rollback

1. Power the console down normally if it remains responsive.
2. Remove only the Key4Play PRX line from the test title's section in `plugins.ini`.
3. Restore the backed-up `plugins.ini` if needed.
4. Keep `enable_hooks=0` or remove `key4play.ini`.
5. Reboot and confirm the title and DS4 work without Key4Play.

Never manually unload the plugin while the game is polling controller input. Do not place it in `[default]`.

## What this plan cannot establish remotely

CI can confirm compilation and the Windows test suite can validate the shared parsers/mapping and jump-plan calculation. Only the actual PS4 can establish:
- Whether the USB API and descriptor handling behave correctly on this firmware/GoldHEN build.
- Whether the expected Pad forwarding-stub signatures are present.
- Whether the atomic patch works in the title process.
- Whether DS4 passthrough, game compatibility and plugin shutdown are stable.
