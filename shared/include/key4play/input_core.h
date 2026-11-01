#ifndef KEY4PLAY_INPUT_CORE_H
#define KEY4PLAY_INPUT_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DualShock-style button bits used by the PS4 Pad data interface. */
#define K4P_BTN_L3       0x00000002u
#define K4P_BTN_R3       0x00000004u
#define K4P_BTN_OPTIONS  0x00000008u
#define K4P_BTN_UP       0x00000010u
#define K4P_BTN_RIGHT    0x00000020u
#define K4P_BTN_DOWN     0x00000040u
#define K4P_BTN_LEFT     0x00000080u
#define K4P_BTN_L2       0x00000100u
#define K4P_BTN_R2       0x00000200u
#define K4P_BTN_L1       0x00000400u
#define K4P_BTN_R1       0x00000800u
#define K4P_BTN_TRIANGLE 0x00001000u
#define K4P_BTN_CIRCLE   0x00002000u
#define K4P_BTN_CROSS    0x00004000u
#define K4P_BTN_SQUARE   0x00008000u
#define K4P_BTN_TOUCHPAD 0x00100000u

#define K4P_AXIS_LX 0x01u
#define K4P_AXIS_LY 0x02u
#define K4P_AXIS_RX 0x04u
#define K4P_AXIS_RY 0x08u

#define K4P_MOD_LEFT_CTRL  0x01u
#define K4P_MOD_LEFT_SHIFT 0x02u

typedef struct K4pKeyboardReport {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} K4pKeyboardReport;

typedef struct K4pMouseReport {
    uint8_t buttons; /* bit 0 = left, bit 1 = right, bit 2 = middle */
    int8_t x;
    int8_t y;
    int8_t wheel;
} K4pMouseReport;

typedef struct K4pSettings {
    float mouse_sensitivity;
    float mouse_deadzone;
    float mouse_acceleration;
} K4pSettings;

typedef struct K4pPadState {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    uint8_t axis_override;
} K4pPadState;

void k4p_settings_default(K4pSettings *settings);
void k4p_pad_state_neutral(K4pPadState *state);

int k4p_decode_boot_keyboard(const uint8_t *data, size_t length,
                             K4pKeyboardReport *report);
int k4p_decode_boot_mouse(const uint8_t *data, size_t length,
                          K4pMouseReport *report);
int k4p_key_down(const K4pKeyboardReport *report, uint8_t usage);

void k4p_map_keyboard(const K4pKeyboardReport *keyboard, K4pPadState *out);
void k4p_map_mouse(const K4pMouseReport *mouse, const K4pSettings *settings,
                   K4pPadState *out);

/* Merge mapped input on top of the real DS4 sample without erasing it. */
void k4p_merge_pad_state(K4pPadState *physical, const K4pPadState *mapped);

#ifdef __cplusplus
}
#endif

#endif
