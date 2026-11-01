# Test Lab v0.2

Default window: 1024x640, minimum 800x520, resizable.

## Default mapping

| Input | DS4 output |
|---|---|
| WASD | Left stick |
| Arrow keys | D-pad |
| Space | Cross |
| Left Ctrl | Circle |
| E / Q | Square / Triangle |
| Left Shift / R | L1 / R1 |
| C / V | L3 / R3 |
| Enter | Options |
| Mouse movement | Right stick |
| Mouse left / right | R2 / L2 |
| Mouse middle | Touchpad click |

PS button remains on the physical DS4.

## Test coverage

Shared C tests validate boot report lengths, keyboard rollover rejection, usage lookup, directional and face-button mapping, mouse signed deltas, trigger/button mapping, axis conversion and preservation of physical controller state during merge.

## Important simulator limitation

SDL state is packed into a standard 6-key boot report for this first simulator. A physical keyboard may support NKRO or non-boot report descriptors; this first-stage parser deliberately rejects boot-protocol rollover rather than guessing. A descriptor-aware HID parser is a separate upcoming task.
