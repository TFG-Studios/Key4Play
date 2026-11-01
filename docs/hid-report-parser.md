# Generic HID report-descriptor parser

The shared C11 layer now has a bounded HID report-descriptor parser and report decoder in:

- `shared/include/key4play/hid_report.h`
- `shared/src/hid_report.c`

It can locate keyboard and mouse input fields inside HID Application collections, track per-Report-ID bit offsets, and decode:

- Keyboard modifier fields and key arrays.
- NKRO keyboard bitmaps expressed as variable fields with a Usage range.
- Mouse buttons 1–3.
- Generic Desktop X, Y and vertical wheel values, including signed fields.

The parser uses fixed-size storage and rejects descriptors over 4096 bytes, more than 512 items, excess global/collection nesting, more than 64 retained fields, report fields wider than 32 bits, and input reports above 128 bytes. It skips HID long items after validating their bounds. It does not allocate memory.

## Deliberate limits

This is a first generic-parser milestone, not support for every HID device. It does not yet decode Consumer Control, horizontal wheel, pan, high-resolution wheel usages, vendor-defined usages, multiple physical mouse interfaces, or arbitrary vendor-specific semantics. It also keeps the existing six-key mapping structure, so a keyboard bitmap with more than six simultaneous non-modifier keys is reduced to the first six decoded usages.

The parser is not yet connected to the PS4 USB reader. The reader still uses boot-protocol reports. Integration should follow after parser fixtures and additional malformed-descriptor tests pass, then the reader can request and parse each device's report descriptor and decode reports using the selected Report ID.

## Tests

The shared test executable includes a synthetic composite descriptor with:
- An NKRO keyboard on Report ID 1.
- A mouse on Report ID 2 with signed X/Y/wheel fields.

The Windows Test Lab CI builds the parser as part of the shared input-core library and runs the fixtures through CTest.
