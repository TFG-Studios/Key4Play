#ifndef KEY4PLAY_HID_REPORT_H
#define KEY4PLAY_HID_REPORT_H

#include <stddef.h>
#include <stdint.h>

#include "key4play/input_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define K4P_HID_MAX_FIELDS 64u
#define K4P_HID_MAX_REPORT_BYTES 128u

typedef enum K4pHidCollection {
    K4P_HID_COLLECTION_NONE = 0,
    K4P_HID_COLLECTION_KEYBOARD = 1,
    K4P_HID_COLLECTION_MOUSE = 2
} K4pHidCollection;

typedef struct K4pHidField {
    uint16_t usage_page;
    uint16_t usage_min;
    uint16_t usage_max;
    uint16_t bit_offset;
    uint16_t report_size;
    uint16_t report_count;
    int32_t logical_min;
    int32_t logical_max;
    uint8_t report_id;
    uint8_t collection;
    uint8_t is_variable;
} K4pHidField;

typedef struct K4pHidLayout {
    K4pHidField fields[K4P_HID_MAX_FIELDS];
    uint8_t field_count;
    uint8_t uses_report_ids;
    uint8_t report_bytes[256];
} K4pHidLayout;

/*
 * Parse a HID report descriptor into a bounded layout.
 * The parser supports report IDs, keyboard arrays and NKRO bitmaps,
 * modifier bits, mouse buttons, X/Y axes and the vertical wheel.
 * Returns 1 on success and 0 for malformed, unsupported or oversized input.
 */
int k4p_hid_parse_report_descriptor(const uint8_t *descriptor, size_t length,
                                    K4pHidLayout *layout);

/* Decode one complete input report selected by its Report ID (when present). */
int k4p_hid_decode_keyboard(const K4pHidLayout *layout,
                            const uint8_t *data, size_t length,
                            K4pKeyboardReport *report);
int k4p_hid_decode_mouse(const K4pHidLayout *layout,
                         const uint8_t *data, size_t length,
                         K4pMouseReport *report);

#ifdef __cplusplus
}
#endif

#endif
